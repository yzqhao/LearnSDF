#include "SDFShared.hlsl"

//Texture2D DepthGbuffer : register(t3, space0);
Texture2D gTextureMaps[200] : register(t3, space0);

struct VertexIn
{
    float3 PosL    : POSITION;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float2 TexC    : TEXCOORD;
};


VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

	// Already in homogeneous clip space.
	vout.PosH = float4(vin.PosL, 1.0f);

    vout.TexC = vin.TexC;

    return vout;
}

float4 NDCToView(float4 NDCPos, float4x4 InvProj)
{
    float4 View = mul(NDCPos, InvProj);
    View /= View.w;
 
    return View;
}

float4 UVToNDC(float2 UVPos, float Depth)
{
    return float4(2 * UVPos.x - 1, 1 - 2 * UVPos.y, Depth, 1.0f);
}

float4 ViewToWolrd(float4 ViewPos, float4x4 InvView)
{
	return mul(ViewPos, InvView);
}

float4 UVToWorld(float2 UV, float NDCDepth, float4x4 InvProj, float4x4 InvView)
{
	float4 NDC = UVToNDC(UV, NDCDepth);
	
	float4 View = NDCToView(NDC, InvProj);
	
	float4 World = ViewToWolrd(View, InvView);
	
	return World;
}

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

void RayMarchDistanceFields(float3 WorldRayStart, float3 WorldRayEnd, float MaxRayTime, out float MinRayTime, out float TotalStepsTaken)
{
	MinRayTime = MaxRayTime;
	TotalStepsTaken = 0;
	
	for (uint i = 0; i < 3; i++)
	{
		if (i == 1)	// 跳过地板的SDF
			continue;
		int SDFIndex = ObjectSDFDescriptors[i].SDFIndex;
		
		float3 LocalRayStart = mul(float4(WorldRayStart, 1.0f), ObjectSDFDescriptors[i].ObjInvWorld).xyz;
		float3 LocalRayEnd = mul(float4(WorldRayEnd, 1.0f), ObjectSDFDescriptors[i].ObjInvWorld).xyz;
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
			
			float MinDistance = 1000000;
			int Step = 0;			
			
			[loop]
			for (; Step < MAX_SDF_STEP; Step++)
			{
				float3 SampleLocalPosition = LocalRayStart + SampleRayTime * LocalRayDir;
				float3 ClampedSamplePosition = clamp(SampleLocalPosition, -LocalExtent, LocalExtent);
				float3 VolumeUV = (ClampedSamplePosition * RcpExtent) * 0.5f + 0.5f;
				float SDFWidth = Extent * 2.0f;
				float DistanceField = SampleMeshDistanceField(SDFIndex, VolumeUV, SDFWidth);
				MinDistance = min(MinDistance, DistanceField);
				
				float MinStepSize = 1.0f / (4 * MAX_SDF_STEP);
				float StepDistance = max(DistanceField, MinStepSize);
				SampleRayTime += StepDistance;
				
				// Terminate the trace if we reached a negative area or went past the end of the ray
				if (DistanceField < 0 || SampleRayTime > IntersectionTimes.y * LocalRayLength)
				{
					break;
				}
			}
			
			if (MinDistance < 0 || Step == MAX_SDF_STEP)
			{
				MinRayTime = min(MinRayTime, SampleRayTime);
			}
				
			TotalStepsTaken += Step;
		}
	}	
}

float4 PS(VertexOut pin) : SV_Target
{
    Texture2D DepthGbuffer = gTextureMaps[18];
	float NDCDepth = DepthGbuffer.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).r;	
	float3 OpaqueWorldPosition = UVToWorld(pin.TexC, NDCDepth, gInvProj, gInvView).xyz;
	
	float TraceDistance = 400;
	float3 WorldRayStart = gEyePosW;
	float3 WorldRayEnd = WorldRayStart + normalize(OpaqueWorldPosition - WorldRayStart) * TraceDistance;
	
	float MinRayTime;
	float TotalStepsTaken;
	RayMarchDistanceFields(WorldRayStart, WorldRayEnd, TraceDistance, MinRayTime, TotalStepsTaken);
	
	float Color = saturate(TotalStepsTaken / 100);
	
	if (MinRayTime < TraceDistance)
	{
		Color += 0.1f;
	}
	
	return float4(Color, Color, Color, 1.0f);
}


