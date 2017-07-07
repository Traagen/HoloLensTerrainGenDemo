#pragma once

#include <float.h>
#include <math.h>

using namespace Windows::Foundation::Numerics;
using namespace DirectX;

namespace MathUtil {
	// A rounding value, or fudge factor for rounding errors.
	static const float EPSILON = 0.000001f;

	struct AABB {
		float3 min; // min x, y, and z values of AABB
		float3 max; // max x, y, and z values of AABB
	};

	// Checks if the supplied Ray (p + d) intersects the triangle (v0, v1, v2).
	// Returns true if so, false if not.
	// Möller–Trumbore ray-triangle intersection algorithm
	float RayTriangleIntersect(float3 p, float3 d, float3 v0, float3 v1, float3 v2);

	// Intersect ray (p, d) with the provided AABB.
	// uses algorithm found in Real Time Collision Detection by Christer Ericson (2005).
	// returns true for intersection, false for no intersection.
	bool RayAABBIntersect(float3 p, float3 d, AABB a);

	// Intersect ray (p, d) with the OBB defined
	// as an AABB and a transformation matrix.
	// returns true for intersection, false for no intersection.
	bool RayOBBIntersect(float3 p, float3 d, AABB a, float4x4 transform);

	// Rotates a 3D vector by a quaterion. Returns the vector in the same variable.
	void RotateVector(XMFLOAT3& vec, XMFLOAT4 quat);
}
