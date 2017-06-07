#include "pch.h"
#include "MathFunctions.h"
#include <utility>

using namespace Windows::Foundation::Numerics;
using namespace DirectX;

// Checks if the supplied Ray (p + d) intersects the triangle (v0, v1, v2).
// Returns true if so, false if not.
// M�ller�Trumbore ray-triangle intersection algorithm
bool MathUtil::RayTriangleIntersect(float3 p, float3 d, float3 v0, float3 v1, float3 v2) {
	// Find vectors for two edges sharing V1
	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;

	// Begin calculating determinant - also used to calculate u parameter
	float3 h = cross(d, e2);
	float det = dot(e1, h);

	// if determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
	if (det > -EPSILON && det < EPSILON) {
		return false;
	}

	float inv_det = 1 / det;

	// calculate distance from V1 to ray origin
	float3 s = p - v0;

	// Calculate u parameter and test bound
	float u = inv_det * dot(s, h);

	if (u < 0.0f || u > 1.0f) {
		// The intersection lies outside of the triangle
		return false;
	}

	// Calculate V parameter and test bound
	float3 q = cross(s, e1);
	float v = inv_det * dot(d, q);

	if (v < 0.0f || u + v > 1.0f) {
		// The intersection lies outside of the triangle
		return false;
	}

	// compute t, the intersection point along the line.
	float t = inv_det * dot(e2, q);

	if (t > EPSILON) {
		// the point of intersection happens on the ray.
		return true;
	}
	else {
		// the point of intersection happens on the line, but not on the ray.
		// ie, it happens before the start of the ray, point p.
		return false;
	}
}

// Intersect ray (p, d) with the provided AABB.
// uses algorithm found in Real Time Collision Detection by Christer Ericson (2005).
// returns true for intersection, false for no interseciton.
bool MathUtil::RayAABBIntersect(float3 p, float3 d, MathUtil::AABB a) {
	float tmin = 0.0f;
	float tmax = FLT_MAX;

	// X slab.
	if (fabsf(p.x) < EPSILON) {
		// ray is parallel to slab. No hit if origin not within slab.
		if (p.x < a.min.x || p.x > a.max.x) {
			return false;
		}
	}
	else {
		// compute intersection t value of ray with near and far plane of slab.
		float ood = 1.0f / d.x;
		float t1 = (a.min.x - p.x) * ood;
		float t2 = (a.max.x - p.x) * ood;

		// make t1 be intersection with near plane, t2 with far plane.
		if (t1 > t2) {
			std::swap(t1, t2);
		}

		// compute the intersection of slab intersection intervals.
		tmin = fmaxf(tmin, t1);
		tmax = fminf(tmax, t2);

		// exit with no collision as soon as slab intersection becomes empty.
		if (tmin > tmax) {
			return false;
		}
	}

	// Y slab.
	if (fabsf(p.y) < EPSILON) {
		// ray is parallel to slab. No hit if origin not within slab.
		if (p.y < a.min.y || p.y > a.max.y) {
			return false;
		}
	}
	else {
		// compute intersection t value of ray with near and far plane of slab.
		float ood = 1.0f / d.y;
		float t1 = (a.min.y - p.y) * ood;
		float t2 = (a.max.y - p.y) * ood;

		// make t1 be intersection with near plane, t2 with far plane.
		if (t1 > t2) {
			std::swap(t1, t2);
		}

		// compute the intersection of slab intersection intervals.
		tmin = fmaxf(tmin, t1);
		tmax = fminf(tmax, t2);

		// exit with no collision as soon as slab intersection becomes empty.
		if (tmin > tmax) {
			return false;
		}
	}

	// Z slab.
	if (fabsf(p.z) < EPSILON) {
		// ray is parallel to slab. No hit if origin not within slab.
		if (p.z < a.min.z || p.z > a.max.z) {
			return false;
		}
	}
	else {
		// compute intersection t value of ray with near and far plane of slab.
		float ood = 1.0f / d.z;
		float t1 = (a.min.z - p.z) * ood;
		float t2 = (a.max.z - p.z) * ood;

		// make t1 be intersection with near plane, t2 with far plane.
		if (t1 > t2) {
			std::swap(t1, t2);
		}

		// compute the intersection of slab intersection intervals.
		tmin = fmaxf(tmin, t1);
		tmax = fminf(tmax, t2);

		// exit with no collision as soon as slab intersection becomes empty.
		if (tmin > tmax) {
			return false;
		}
	}

	// ray intersects all 3 slabs.
	return true;
}

// Rotates a 3D vector by a quaterion. Returns the vector in the same variable.
void MathUtil::RotateVector(XMFLOAT3& vector, XMFLOAT4 quaternion) {
	XMVECTOR rotation = XMLoadFloat4(&quaternion);
	XMVECTOR vec = XMLoadFloat3(&vector);
	XMVECTOR result = XMVector3Rotate(vec, rotation);
	XMStoreFloat3(&vector, result);
}