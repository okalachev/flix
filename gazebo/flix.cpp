// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// https://classic.gazebosim.org/tutorials?tut=plugins_model&cat=write_plugin
// https://classic.gazebosim.org/tutorials?tut=set_velocity&cat=
// https://github.com/gazebosim/gazebo-classic/blob/gazebo11/plugins/ArduCopterPlugin.cc
// https://github.com/gazebosim/gazebo-classic/blob/gazebo11/plugins/ArduCopterPlugin.cc#L510 - motor
// https://classic.gazebosim.org/tutorials?tut=gui_overlay&cat=user_input
// https://github.com/gazebosim/gazebo-classic/blob/gazebo9/examples/plugins/gui_overlay_plugin_time/GUIExampleTimeWidget.cc
// https://github.com/yujinrobot/kobuki_desktop/blob/ea5b7283d92f61efbd1a2185b46e1ad344e7e81a/kobuki_gazebo_plugins/src/gazebo_ros_kobuki_loads.cpp#L29
// https://github.com/osrf/swarm/blob/1a2e4040b12b686ed7a13e32301d538b1c7d0b1d/src/RobotPlugin.cc#L936

// motors thrust: https://www.youtube.com/watch?v=VtKI4Pjx8Sk

// https://github.com/gazebosim/gazebo-classic/tree/master/examples/plugins

// publish to topics https://github.com/wuwushrek/sim_cf/blob/df68af275c9f753d9bf1b0494a4e513d9f4c9a7c/crazyflie_gazebo/src/gazebo_lps_plugin.cpp#L104
// https://github.com/bitcraze/crazyflie-simulation

// GUI overlay:
// https://github.com/gazebosim/gazebo-classic/blob/gazebo9/examples/plugins/gui_overlay_plugin_time/GUIExampleTimeWidget.cc

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

#include "arduino.hpp"
#include "flix.hpp"
#include "turnigy.hpp"
#include "util.ino"
#include "estimate.ino"
#include "control.ino"

using ignition::math::Vector3d;
using ignition::math::Pose3d;
using namespace gazebo;
using namespace std;

Pose3d flu2frd(const Pose3d& p)
{
	return ignition::math::Pose3d(p.Pos().X(), -p.Pos().Y(), -p.Pos().Z(),
	                              p.Rot().W(), p.Rot().X(), -p.Rot().Y(), -p.Rot().Z());
}

class ModelFlix : public ModelPlugin
{
private:
	physics::ModelPtr model, estimateModel;
	physics::LinkPtr body;
	sensors::ImuSensorPtr imu;
	common::Time _stepTime;
	event::ConnectionPtr updateConnection, resetConnection;
	transport::NodePtr nodeHandle;
	transport::PublisherPtr motorPub[4];
	ofstream log;

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

		// auto scene = rendering::get_scene();
		// motorVisual[0] = scene->GetVisual("motor0");
		// motorVisual[1] = scene->GetVisual("motor1");
		// motorVisual[2] = scene->GetVisual("motor2");
		// motorVisual[3] = scene->GetVisual("motor3");
		// motorVisual[0] = model->GetVisual("motor0");

		this->updateConnection = event::Events::ConnectWorldUpdateBegin(
			std::bind(&ModelFlix::OnUpdate, this));

		this->resetConnection = event::Events::ConnectWorldReset(
			std::bind(&ModelFlix::OnReset, this));

		initNode();

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
		// this->model->SetLinearVel(ignition::math::Vector3d(.3, 0, 0));

		// this->model->GetLink("body")->AddForce(ignition::math::Vector3d(0.0, 0.0, 1.96));
		// this->model->GetLink("body")->SetTorque(1.0, ignition::math::Vector3d(1.0, 0.0, 0.0);

		// this->model->GetLink("motor0")->AddForce(ignition::math::Vector3d(0.0, 0.0, 1.96/4));
		// this->model->GetLink("motor1")->AddForce(ignition::math::Vector3d(0.0, 0.0, 1.96/4));
		// this->model->GetLink("motor2")->AddForce(ignition::math::Vector3d(0.0, 0.0, 1.96/4));
		// this->model->GetLink("motor3")->AddForce(ignition::math::Vector3d(0.0, 0.0, 1.96/4))

		// === GROUNDTRUTH ORIENTATION ===
		// auto pose = flu2frd(this->model->WorldPose());
		// attitude = Quaternion(pose.Rot().W(), pose.Rot().X(), pose.Rot().Y(), pose.Rot().Z());
		// auto vel = this->model->RelativeAngularVel();
		// rates = Vector(vel.X(), -vel.Y(), -vel.Z()); // flu to frd

		// === GROUNDTRUTH POSITION ===
		// auto pose = flu2frd(this->model->WorldPose());
		// position = Vector(pose.Pos().X(), pose.Pos().Y(), pose.Pos().Z());
		// auto vel = this->model->RelativeLinearVel();
		// velocity = Vector(vel.X(), -vel.Y(), -vel.Z());

		/// == IMU ===
		auto imuRates = imu->AngularVelocity();
		rates = Vector(imuRates.X(), -imuRates.Y(), -imuRates.Z()); // flu to frd
		auto imuAccel = imu->LinearAcceleration();
		acc = Vector(imuAccel.X(), -imuAccel.Y(), -imuAccel.Z()); // flu to frd

		// gazebo::common::Time curTime = this->dataPtr->model->GetWorld()->SimTime();

		turnigyGet();
		controls[RC_CHANNEL_MODE] = 1; // 0 acro, 1 stab
		controls[RC_CHANNEL_AUX] = 1; // armed
		// std::cout << "yaw: " << yaw << " pitch: " << pitch << " roll: " << roll << " throttle: " << throttle << std::endl;

		if (std::isnan(dt)) {
			// first step
			dt = 0;
		} else {
			dt = (this->model->GetWorld()->SimTime() - _stepTime).Double();
		}
		_stepTime = this->model->GetWorld()->SimTime();
		stepTime = _stepTime.Double() * 1000000;
		// std::cout << "dt: " << dt << std::endl;

		estimate();

		// fix yaw
		attitude.setYaw(-this->model->WorldPose().Yaw());
		// Serial.print("attitude: "); Serial.println(attitude);
		// controlMission();
		// controlPosition();

		// auto pose = flu2frd(this->model->WorldPose());

		control();
		applyMotorsThrust();
		updateEstimatePose();
		// autotune();

		/*
		working:
		const double max_thrust = 2.2;
		double thrust = max_thrust * throttle;

		// rate setpoint
		double rx = roll * 0.1;
		double ry = -pitch * 0.1;
		double rz = yaw * 0.1;

		// this->model->GetLink("body")->AddForce(ignition::math::Vector3d(0.0, 0.0, 2.5 * throttle));

		const double d = 0.035355;
		double front_left = thrust + ry + rx;
		double front_right = thrust + ry - rx;
		double rear_left = thrust - ry + rx;
		double rear_right = thrust - ry - rx;

		if (throttle < 0.1) return;

		this->body->AddLinkForce(ignition::math::Vector3d(0.0, 0.0, front_left), ignition::math::Vector3d(d, d, 0.0));
		this->body->AddLinkForce(ignition::math::Vector3d(0.0, 0.0, front_right), ignition::math::Vector3d(d, -d, 0.0));
		this->body->AddLinkForce(ignition::math::Vector3d(0.0, 0.0, rear_left), ignition::math::Vector3d(-d, d, 0.0));
		this->body->AddLinkForce(ignition::math::Vector3d(0.0, 0.0, rear_right), ignition::math::Vector3d(-d, -d, 0.0));
		*/

		publishTopics();
		logData();
	}

	void applyMotorsThrust()
	{
		// thrusts
		const double d = 0.035355;
		const double maxThrust = 0.03 * ONE_G; // 30 g, https://www.youtube.com/watch?v=VtKI4Pjx8Sk
		// 65 mm prop ~40 g

		// std::cout << "fr: " << motors[MOTOR_FRONT_RIGHT]
		// 	<< " fl: " << motors[MOTOR_FRONT_LEFT]
		// 	<< " rr: " << motors[MOTOR_REAR_RIGHT]
		// 	<< " rl: " << motors[MOTOR_REAR_LEFT] << std::endl;

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

		// const float scale0 = 1.0, scale1 = 1.0, scale2 = 1.0, scale3 = 1.0;

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

		float angle = Vector::angleBetweenVectors(attitude.rotate(Vector(0, 0, -1)), Vector(0, 0, -1));
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
		nodeHandle->Init(); // TODO: namespace
		motorPub[0] = nodeHandle->Advertise<msgs::Int>("~/motor0");
		motorPub[1] = nodeHandle->Advertise<msgs::Int>("~/motor1");
		motorPub[2] = nodeHandle->Advertise<msgs::Int>("~/motor2");
		motorPub[3] = nodeHandle->Advertise<msgs::Int>("~/motor3");
	}

	void publishTopics() {
		for (int i = 0; i < 4; i++) {
			msgs::Int msg;
			msg.set_data(static_cast<int>(std::round(motors[i] * 1000)));
			motorPub[i]->Publish(msg);
		}
	}

	void logData() {
		if (!log.is_open()) return;
		log << this->model->GetWorld()->SimTime() << "\t" << rollRatePID.derivative << "\t" << pitchRatePID.derivative << "\n";
	}
};

GZ_REGISTER_MODEL_PLUGIN(ModelFlix)
