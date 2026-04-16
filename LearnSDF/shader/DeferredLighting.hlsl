#include "Shadows.hlsl"
#include "PBRLighting.hlsl"

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


float4 PS(VertexOut pin) : SV_TARGET
{
	float3 FinalColor = 0.0f;
	
    int texIdx = 13;
    Texture2D BaseColorGbuffer = gTextureMaps[texIdx++];
    Texture2D NormalGbuffer = gTextureMaps[texIdx++];
    Texture2D WorldPosGbuffer = gTextureMaps[texIdx++];
    Texture2D OrmGbuffer = gTextureMaps[texIdx++];
    //Texture2D EmissiveGbuffer = gTextureMaps[17];

	// Get Gbuffer data
	float3 BaseColor = BaseColorGbuffer.Sample(gsamPointClamp, pin.TexC).rgb;	
	float3 Normal = NormalGbuffer.Sample(gsamPointClamp, pin.TexC).rgb;			
	float3 WorldPos = WorldPosGbuffer.Sample(gsamPointClamp, pin.TexC).rgb;
	float ShadingModelValue = WorldPosGbuffer.Sample(gsamPointClamp, pin.TexC).a;
	uint ShadingModel = (uint)round(ShadingModelValue * (float)0xF);	
	float Roughness = OrmGbuffer.Sample(gsamPointClamp, pin.TexC).g;
	float Metallic = OrmGbuffer.Sample(gsamPointClamp, pin.TexC).b;	
    float3 EmissiveColor = float3(0, 0, 0); //EmissiveGbuffer.Sample(gsamPointClamp, pin.TexC).rgb;

	float3 CameraPosition = gEyePosW;
	float3 ViewDir = normalize(CameraPosition - WorldPos);
	Normal = normalize(Normal);
		
	// Emissive light
	FinalColor += EmissiveColor;
		
	[unroll(10)]
	for (uint i = 0; i < LightCount; i++)
	{
		LightParameters Light = Lights[i];
		// Directional light
		{
			// Get shadow factor        
			float4 ShadowPosH = mul(float4(WorldPos, 1.0f), Light.ShadowTransform);
			float3 LightDir = normalize(-Light.Direction);
				
			// Note: Don't use the positon of DirectionalLightActor,
			// because we should keep the trace direction of SDF shadow always equal to the inverse light direction
			float3 LightPosition = WorldPos + LightDir * 100.0f;
				
			float TanLightAngle = tan(3 * PI / 180.0f);  //TODO
			float ShadowFactor = CalcVisibility(ShadowPosH, false, WorldPos, LightPosition, TanLightAngle); 						
				
			float3 Radiance = Light.Intensity * Light.Color;

			FinalColor += DirectLighting(Radiance, LightDir, Normal, ViewDir, Roughness, Metallic, BaseColor, ShadowFactor);	
		}
	}

    return float4(FinalColor, 1.0f);
}