#include "ofxFilterUtils.h"

void decomposeMat4x4(glm::mat4x4& _mat, glm::vec3& _outPosition, glm::quat& _outQuaternion) {
	// represents translation, then rotation
	_outPosition = glm::vec3(_mat[3][0], _mat[3][1], _mat[3][2]);
	_outQuaternion = glm::quat_cast(_mat); // toQuat?
}

glm::quat quatConvert(ofQuaternion& _q) {
	return glm::quat(_q.w(), _q.x(), _q.y(), _q.z()); // w,x,y,z
}

void composeMat4x4(glm::vec3& _position, glm::quat& _quaternion, glm::mat4x4& _outMat) {
	// translation, then rotation
	glm::mat4x4 posMat = glm::translate(_position);
	glm::mat4x4 rotMat = glm::toMat4(_quaternion);
	_outMat = posMat * rotMat;
}

glm::vec3 getTranslation(glm::mat4x4& m) { 
	return { m[3][0], m[3][1], m[3][2] }; 
}

glm::quat getRotation(glm::mat4x4& m) { 
	return glm::quat_cast(m); 
}

glm::vec3 getScale(glm::mat4x4& m) {
	return glm::vec3(
		glm::l2Norm(glm::vec3(m[0][0], m[0][1], m[0][2])),
		glm::l2Norm(glm::vec3(m[1][0], m[1][1], m[1][2])),
		glm::l2Norm(glm::vec3(m[2][0], m[2][1], m[2][2])));
}

glm::vec3 getXAxis(glm::mat4x4& a) {
	return glm::vec3(a[0][0], a[0][1], a[0][2]);
}

glm::vec3 getYAxis(glm::mat4x4& a) {
	return glm::vec3(a[1][0], a[1][1], a[1][2]);
}

glm::vec3 getZAxis(glm::mat4x4& a) {
	return glm::vec3(a[2][0], a[2][1], a[2][2]);
}

glm::vec3 getEulerWarped(glm::vec3 euler, glm::vec3 refEuler) {

	for (int i = 0; i < 3; i++) {
		float rev = floorf((refEuler[i] + 180) / 360.f) * 360;
		euler[i] += rev;
		if (euler[i] < -90 + rev && refEuler[i] > 90 + rev) euler[i] += 360;
		else if (euler[i] > 90 + rev && refEuler[i] < -90 + rev) euler[i] -= 360;
	}
	return euler;
}
