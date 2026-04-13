
#pragma once

#include "Vec3.h"

NS_JYE_MATH_BEGIN

class AABB;

class Ray
{
public:     
    Vec3 origin;   
    Vec3 direction;
    mutable float MaxDist;
public:
    Ray(): MaxDist(FLT_MAX){}
    Ray(const Vec3& origin, const Vec3& direction, float InMaxDist = FLT_MAX) : origin(origin), direction(direction), MaxDist(InMaxDist) {}

	void SetRayOrigin(const Vec3& ori) { origin = ori; }
	void SetRayDirection(const Vec3& dir) { direction = dir; }
	const Vec3& GetRayOrigin() const { return origin; }
	const Vec3& GetRayDirection() const { return direction; }
	bool Intersect(const Math::AABB& box) const;
};

NS_JYE_MATH_END
