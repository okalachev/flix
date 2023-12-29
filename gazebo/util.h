#include <ignition/math/Vector3.hh>
#include <ignition/math/Pose3.hh>

using ignition::math::Vector3d;
using ignition::math::Pose3d;

Pose3d flu2frd(const Pose3d& p) {
	return ignition::math::Pose3d(p.Pos().X(), -p.Pos().Y(), -p.Pos().Z(),
	                              p.Rot().W(), p.Rot().X(), -p.Rot().Y(), -p.Rot().Z());
}

Vector flu2frd(const Vector3d& v) {
	return Vector(v.X(), -v.Y(), -v.Z());
}
