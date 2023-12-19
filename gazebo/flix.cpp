// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Gazebo plugin for running Arduino code and simulating the drone

#include <functional>
#include <cmath>
#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/rendering/rendering.hh>
#include <gazebo/common/common.hh>
#include <gazebo/sensors/sensors.hh>
#include <gazebo/msgs/msgs.hh>
#include <ignition/math/Vector3.hh>
#include <ignition/math/Pose3.hh>
#include <ignition/math/Quaternion.hh>
#include <iostream>
#include <fstream>

#include "Arduino.h"
#include "flix.h"
#include "util.ino"
#include "joystick.h"
#include "time.ino"
#include "estimate.ino"
#include "control.ino"
#include "log.ino"
#include "cli.ino"
#include "debug.ino"
#include "lpf.h"

using ignition::math::Vector3d;
using ignition::math::Pose3d;
using namespace gazebo;
using namespace std;

Pose3d flu2frd(const Pose3d& p)
{
	return ignition::math::Pose3d(p.Pos().X(), -p.Pos().Y(), -p.Pos().Z(),
	                              p.Rot().W(), p.Rot().X(), -p.Rot().Y(), -p.Rot().Z());
}

Vector flu2frd(const Vector3d& v)
{
	return Vector(v.X(), -v.Y(), -v.Z());
}

class ModelFlix : public ModelPlugin
{
private:
	physics::ModelPtr model, estimateModel;
	physics::LinkPtr body;
	sensors::ImuSensorPtr imu;
	event::ConnectionPtr updateConnection, resetConnection;
	transport::NodePtr nodeHandle;
	transport::PublisherPtr motorPub[4];
	LowPassFilter<Vector> accFilter = LowPassFilter<Vector>(0.1);

public:
	void Load(physics::ModelPtr _parent, sdf::ElementPtr /*_sdf*/)
	{
		this->model = _parent;
		this->body = this->model->GetLink("body");

		this->imu = std::dynamic_pointer_cast<sensors::ImuSensor>(sensors::get_sensor(model->GetScopedName(true) + "::body::imu")); // default::flix::body::imu
		if (imu == nullptr) {
			gzerr << "IMU sensor not found" << std::endl;
			return;
		}

		this->estimateModel = model->GetWorld()->ModelByName("flix_estimate");

		this->updateConnection = event::Events::ConnectWorldUpdateBegin(
			std::bind(&ModelFlix::OnUpdate, this));

		this->resetConnection = event::Events::ConnectWorldReset(
			std::bind(&ModelFlix::OnReset, this));

		initNode();

		Serial.begin(0);

		gzmsg << "Flix plugin loaded" << endl;
	}

public:
	void OnReset()
	{
		attitude = Quaternion();
		gzmsg << "Flix plugin reset" << endl;
	}

	void OnUpdate()
	{
		__micros = model->GetWorld()->SimTime().Double() * 1000000;
		step();

		// read imu
		rates = flu2frd(imu->AngularVelocity());
		acc = this->accFilter.update(flu2frd(imu->LinearAcceleration()));

		// read rc
		joystickGet();
		controls[RC_CHANNEL_MODE] = 1; // 0 acro, 1 stab
		controls[RC_CHANNEL_AUX] = 1; // armed

		estimate();

		// correct yaw to the actual yaw
		attitude.setYaw(-this->model->WorldPose().Yaw());

		control();
		parseInput();

		applyMotorsThrust();
		updateEstimatePose();
		publishTopics();
		logData();
	}

	void applyMotorsThrust()
	{
		// thrusts
		const double d = 0.035355;
		const double maxThrust = 0.03 * ONE_G; // 30 g, https://www.youtube.com/watch?v=VtKI4Pjx8Sk
		// 65 mm prop ~40 g

		const float scale0 = 1.0, scale1 = 1.1, scale2 = 0.9, scale3 = 1.05;
		const float minThrustRel = 0;

		// apply min thrust
		float mfl = mapff(motors[MOTOR_FRONT_LEFT], 0, 1, minThrustRel, 1);
		float mfr = mapff(motors[MOTOR_FRONT_RIGHT], 0, 1, minThrustRel, 1);
		float mrl = mapff(motors[MOTOR_REAR_LEFT], 0, 1, minThrustRel, 1);
		float mrr = mapff(motors[MOTOR_REAR_RIGHT], 0, 1, minThrustRel, 1);

		if (motors[MOTOR_FRONT_LEFT] < 0.001) mfl = 0;
		if (motors[MOTOR_FRONT_RIGHT] < 0.001) mfr = 0;
		if (motors[MOTOR_REAR_LEFT] < 0.001) mrl = 0;
		if (motors[MOTOR_REAR_RIGHT] < 0.001) mrr = 0;

		// TODO: min_thrust

		body->AddLinkForce(Vector3d(0.0, 0.0, scale0 * maxThrust * abs(mfl)), Vector3d(d, d, 0.0));
		body->AddLinkForce(Vector3d(0.0, 0.0, scale1 * maxThrust * abs(mfr)), Vector3d(d, -d, 0.0));
		body->AddLinkForce(Vector3d(0.0, 0.0, scale2 * maxThrust * abs(mrl)), Vector3d(-d, d, 0.0));
		body->AddLinkForce(Vector3d(0.0, 0.0, scale3 * maxThrust * abs(mrr)), Vector3d(-d, -d, 0.0));

		// TODO: indicate if > 1

		// torque
		const double maxTorque = 0.0023614413; // 24.08 g*cm
		int direction = 1;
		// z is counter clockwise, normal rotation direction is minus
		body->AddRelativeTorque(Vector3d(0.0, 0.0, direction * scale0 * maxTorque * motors[MOTOR_FRONT_LEFT]));
		body->AddRelativeTorque(Vector3d(0.0, 0.0, direction * scale1 * -maxTorque * motors[MOTOR_FRONT_RIGHT]));
		body->AddRelativeTorque(Vector3d(0.0, 0.0, direction * scale2 * -maxTorque * motors[MOTOR_REAR_LEFT]));
		body->AddRelativeTorque(Vector3d(0.0, 0.0, direction * scale3 * maxTorque * motors[MOTOR_REAR_RIGHT]));
	}

	void updateEstimatePose() {
		if (estimateModel == nullptr) {
			return;
		}
		if (!attitude.finite()) {
			// gzerr << "attitude is nan" << std::endl;
			return;
		}
		Pose3d pose(
			model->WorldPose().Pos().X(), model->WorldPose().Pos().Y(), model->WorldPose().Pos().Z(),
			attitude.w, attitude.x, -attitude.y, -attitude.z // frd to flu
		);
		// std::cout << pose.Pos().X() << " " << pose.Pos().Y() << " " << pose.Pos().Z() <<
		// 	" " << pose.Rot().W() << " " << pose.Rot().X() << " " << pose.Rot().Y() << " " << pose.Rot().Z() << std::endl;

		// calculate attitude estimation error

		Quaternion groundtruthAttitude(estimateModel->WorldPose().Rot().W(), estimateModel->WorldPose().Rot().X(), -estimateModel->WorldPose().Rot().Y(), -estimateModel->WorldPose().Rot().Z());
		float angle = Vector::angleBetweenVectors(attitude.rotate(Vector(0, 0, -1)), groundtruthAttitude.rotate(Vector(0, 0, -1)));
		if (angle < 0.3) {
			//gzwarn << "att err: " << angle << endl;
			// TODO: warning
			// position under the floor to make it invisible
			pose.SetZ(-5);
		}

		estimateModel->SetWorldPose(pose);

	}

	void initNode() {
		nodeHandle = transport::NodePtr(new transport::Node());
		nodeHandle->Init();
		string ns = "~/" + model->GetName();
		motorPub[0] = nodeHandle->Advertise<msgs::Int>(ns + "/motor0");
		motorPub[1] = nodeHandle->Advertise<msgs::Int>(ns + "/motor1");
		motorPub[2] = nodeHandle->Advertise<msgs::Int>(ns + "/motor2");
		motorPub[3] = nodeHandle->Advertise<msgs::Int>(ns + "/motor3");
	}

	void publishTopics() {
		for (int i = 0; i < 4; i++) {
			msgs::Int msg;
			msg.set_data(static_cast<int>(std::round(motors[i] * 1000)));
			motorPub[i]->Publish(msg);
		}
	}
};

GZ_REGISTER_MODEL_PLUGIN(ModelFlix)
