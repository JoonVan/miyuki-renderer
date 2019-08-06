#pragma once
#include "vec3.hpp"

namespace Miyuki {
	struct Vec4f : Vec3f {
		Vec4f() :Vec4f(0.0) {}
		Vec4f(Float x) {
			v[0] = v[1] = v[2] = v[3] = x;
		}
		Vec4f(Float x, Float y, Float z, Float w) {
			v[0] = x;
			v[1] = y;
			v[2] = z;
			v[3] = w;
		}
		Vec4f(const Vec3f& vec, Float w) {
			v[0] = vec.v[0];
			v[1] = vec.v[1];
			v[2] = vec.v[2];
			v[3] = w;
		}
		static Float dot(const Vec4f& a, const Vec4f& b) {
			return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		}

		static Float absDot(const Vec4f& a, const Vec4f& b) {
			return fabs(dot(a, b));
		}
		operator Vec3f()const {
			return Vec3f(v[0], v[1], v[2]);  
		}
		bool operator==(const Vec4f& rhs) const {
			return v[0] == rhs.v[0] && v[1] == rhs.v[1] && v[2] == rhs.v[2] && v[3] == rhs.v[3];
		}

		bool operator!=(const Vec4f& rhs) const {
			return !(*this == rhs);
		}

		Float max() const {
			return std::max(std::max(x, std::max(y, z)), w);
		}

		Float min() const {
			return std::min(std::min(x, std::min(y, z)), w);
		}
		Float lengthSquared() const {
			return Vec4f::dot(*this, *this);
		}

		Float length() const{
			return sqrt(lengthSquared());
		}

		void normalize() {
			*this /= length();
		}
	};
}