#ifndef __SHADER_SHADOW__
#define __SHADER_SHADOW__

#include "Common.hlsl"
#include "LightingUtil.hlsl"
#include "SDFShared.hlsl"

StructuredBuffer<LightParameters> Lights : register(t2, space1);

/*
* Clips a ray to an AABB.  Does not handle rays parallel to any of the planes.
*
* @param RayOrigin - The origin of the ray in world space.
* @param RayEnd - The end of the ray in world space.
* @param BoxMin - The minimum extrema of the box.
* @param BoxMax - The maximum extrema of the box.
* @return - Returns the closest intersection along the ray in x, and furthest in y.
*			If the ray did not intersect the box, then the furthest intersection <= the closest intersection.
*			The intersections will always be in the range [0,1], which corresponds to [RayOrigin, RayEnd] in worldspace.
*			To find the world space position of either intersection, simply plug it back into the ray equation:
*			WorldPos = RayOrigin + (RayEnd - RayOrigin) * Intersection;
*/
float2 LineBoxIntersect(float3 RayOrigin, float3 RayEnd, float3 BoxMin, float3 BoxMax)
{
    float3 InvRayDir = 1.0f / (RayEnd - RayOrigin);

	//find the ray intersection with each of the 3 planes defined by the minimum extrema.
    float3 FirstPlaneIntersections = (BoxMin - RayOrigin) * InvRayDir;
	//find the ray intersection with each of the 3 planes defined by the maximum extrema.
    float3 SecondPlaneIntersections = (BoxMax - RayOrigin) * InvRayDir;
	//get the closest of these intersections along the ray
    float3 ClosestPlaneIntersections = min(FirstPlaneIntersections, SecondPlaneIntersections);
	//get the furthest of these intersections along the ray
    float3 FurthestPlaneIntersections = max(FirstPlaneIntersections, SecondPlaneIntersections);

    float2 BoxIntersections;
	//find the furthest near intersection
    BoxIntersections.x = max(ClosestPlaneIntersections.x, max(ClosestPlaneIntersections.y, ClosestPlaneIntersections.z));
	//find the closest far intersection
    BoxIntersections.y = min(FurthestPlaneIntersections.x, min(FurthestPlaneIntersections.y, FurthestPlaneIntersections.z));
	//clamp the intersections to be between RayOrigin and RayEnd on the ray
    return saturate(BoxIntersections);
}

float SDF(float3 ReceiverPosW, float3 LightPosW, float TanLightAngle)
{
	float MinConeVisibility = 1.0f;
	
	for (uint i = 0; i < 3; i++)
	{
	    int SDFIndex = ObjectSDFDescriptors[i].SDFIndex;
		
		float3 LocalRayStart = mul(float4(ReceiverPosW, 1.0f), ObjectSDFDescriptors[i].ObjInvWorld).xyz;
		float3 LocalRayEnd = mul(float4(LightPosW, 1.0f), ObjectSDFDescriptors[i].ObjInvWorld).xyz;
		float3 LocalRayDir = LocalRayEnd - LocalRayStart;
		LocalRayDir = normalize(LocalRayDir);
		float LocalRayLength = length(LocalRayEnd - LocalRayStart);			
		
		float Extent = MeshSDFDescriptors[SDFIndex].Extent;
		int Resolution = MeshSDFDescriptors[SDFIndex].Resolution;
		float RcpExtent = rcp(Extent);
		
		float3 LocalExtent = float3(Extent, Extent, Extent);
		float2 IntersectionTimes = LineBoxIntersect(LocalRayStart, LocalRayEnd, -LocalExtent, LocalExtent);
		
		[branch]
		if (IntersectionTimes.x < IntersectionTimes.y && IntersectionTimes.x < 1)
		{	
			float SampleRayTime = IntersectionTimes.x * LocalRayLength;
			
			int Step = 0;			
			
			[loop]
			for (; Step < MAX_SDF_STEP; Step++)
			{
				float3 SampleLocalPosition = LocalRayStart + SampleRayTime * LocalRayDir;
				float3 ClampedSamplePosition = clamp(SampleLocalPosition, -LocalExtent, LocalExtent);
				float3 VolumeUV = (ClampedSamplePosition * RcpExtent) * 0.5f + 0.5f;
				float SDFWidth = Extent * 2.0f;
				float DistanceField = SampleMeshDistanceField(SDFIndex, VolumeUV, SDFWidth);

				// Don't allow occlusion within an object's self shadow distance
				float SelfShadowScale = 100.0f;
				float SelfShadowVisibility = 1 - saturate(SampleRayTime * SelfShadowScale);
				
				float SphereRadius = TanLightAngle * SampleRayTime;
				float StepConeVisibility = max(saturate(DistanceField / SphereRadius), SelfShadowVisibility);			
				MinConeVisibility = min(MinConeVisibility, StepConeVisibility);							
				
				float MinStepSize = 1.0f / (4 * MAX_SDF_STEP);
				float StepDistance = max(DistanceField, MinStepSize);
				SampleRayTime += StepDistance;
				
				// Terminate the trace if we reached a negative area or went past the end of the ray
				if (DistanceField < 0 || SampleRayTime > IntersectionTimes.y * LocalRayLength)
				{
					break;
				}
			}
		}
	}
	
	if(MinConeVisibility > 0.99f)
	{
		return 1.0f;
	}
	
	return MinConeVisibility;
}

float CalcVisibility(float4 ShadowPosH, bool bPerspectiveView, float3 ReceiverPosW, float3 LightPosW, float TanLightAngle)
{
    // Complete projection by doing division by w.
    ShadowPosH.xyz /= ShadowPosH.w;

    // NDC space.
	float3 ReceiverPos = ShadowPosH.xyz;	
	if (ReceiverPos.x < 0.0f || ReceiverPos.x > 1.0f || ReceiverPos.y < 0.0f || ReceiverPos.y > 1.0f)
	{
		return 0.0f;
	}

	return SDF(ReceiverPosW, LightPosW, TanLightAngle);
}

#endif //__SHADER_SHADOW__
