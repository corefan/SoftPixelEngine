/*
 * Deferred D3D11 shader file
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

/*

Compilation options:

SHADOW_MAPPING  -> Enables shadow mapping.
BLOOM_FILTER    -> Enables bloom filter.
FLIP_Y_AXIS     -> Flips Y axis for OpenGL FBOs.
DEBUG_GBUFFER   -> Renders g-buffer for debugging.

*/


/*
 * ======= Vertex shader: =======
 */

/* === Structures === */

struct SVertexInput
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct SVertexOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
    float4 ViewRay : TEXCOORD1;
};


/* === Uniforms === */

cbuffer BufferVertex : register(b0)
{
    float4x4 ProjectionMatrix;
    float4x4 InvViewProjection;
};


/* === Functions === */

void Frustum(inout float4 v)
{
    v.x = (v.x - 0.5) * 2.0;
    v.y = (v.y - 0.5) * 2.0;
}

SVertexOutput VertexMain(SVertexInput In)
{
    SVertexOutput Out = (SVertexOutput)0;
    
    /* Process vertex transformation for position and normal */
    Out.Position = mul(ProjectionMatrix, float4(In.Position, 1.0));
    Out.TexCoord = In.TexCoord;
    
	/* Pre-compute view ray */
    Out.ViewRay = float4(In.TexCoord.x, 1.0 - In.TexCoord.y, 1.0, 1.0);
	
    Frustum(Out.ViewRay);
	
	Out.ViewRay = mul(InvViewProjection, Out.ViewRay);

    return Out;
}


/*
 * ======= Pixel shader: =======
 */

/* === Structures === */

struct SPixelInput
{
    float2 TexCoord : TEXCOORD0;
    float4 ViewRay : TEXCOORD1;
};

struct SPixelOutput
{
    float4 Color : SV_Target0;
    #ifdef BLOOM_FILTER
    float4 Specular : SV_Target1;
    #endif
};


/* === Uniforms === */

SAMPLER2D(DiffuseAndSpecularMap, 0);
SAMPLER2D(NormalAndDepthMap, 1);

#ifdef SHADOW_MAPPING
SAMPLER2DARRAY(DirLightShadowMaps, 2);
SAMPLERCUBEARRAY(PointLightShadowMaps, 3);
#endif

cbuffer BufferMain : register(b1)
{
    float4x4 ViewTransform  : packoffset(c0);   //!< Global camera transformation.
    float3 ViewPosition     : packoffset(c4);   //!< Global camera position.
    float3 AmbientColor     : packoffset(c5);   //!< Ambient light color.
    float ScreenWidth       : packoffset(c6.x); //!< Screen resolution width.
    float ScreenHeight      : packoffset(c6.y); //!< Screen resolution height.
};

cbuffer BufferLight : register(b2)
{
    int LightCount;
    int LightExCount;

    SLight Lights[MAX_LIGHTS];
    SLightEx LightsEx[MAX_EX_LIGHTS];
};


/* === Functions === */

SPixelOutput PixelMain(SPixelInput In)
{
    SPixelOutput Out = (SPixelOutput)0;

    /* Get texture colors */
	float4 DiffuseAndSpecular = tex2D(DiffuseAndSpecularMap, In.TexCoord);
    float4 NormalAndDepthDist = tex2D(NormalAndDepthMap, In.TexCoord);
	
    /* Compute global pixel position (world space) */
	float3 ViewRayNorm = normalize(In.ViewRay.xyz);
    float3 WorldPos = ViewPosition + ViewRayNorm * NormalAndDepthDist.a;
	
    /* Compute light shading */
	#ifdef HAS_LIGHT_MAP
    float3 StaticDiffuseLight = 0.0;
    float3 StaticSpecularLight = 0.0;
    #endif
	
    float3 DiffuseLight = AmbientColor;
    float3 SpecularLight = 0.0;
	
    for (int i = 0, j = 0; i < LightCount; ++i)
    {
		ComputeLightShading(
			Lights[i], LightsEx[j], WorldPos.xyz, NormalAndDepthDist.xyz, 90.0, ViewRayNorm,
			#ifdef HAS_LIGHT_MAP
			StaticDiffuseLight, StaticSpecularLight,
			#endif
			DiffuseLight, SpecularLight
		);
        
        if (Lights[i].Type != LIGHT_POINT)
            ++j;
    }
	
	#ifdef HAS_LIGHT_MAP
	
	/* Mix light shading with light-map illumination */
	float Illumination = tex2D(IlluminationMap, TexCoord).r;
	
	DiffuseLight += (StaticDiffuseLight * Illumination);
	SpecularLight += (StaticSpecularLight * Illumination);
	
	#endif
	
	#ifndef ALLOW_OVERBLENDING
	DiffuseLight = saturate(DiffuseLight);
	SpecularLight = saturate(SpecularLight);
	#endif
	
    DiffuseLight *= DiffuseAndSpecular.rgb;
    SpecularLight *= DiffuseAndSpecular.a;
	
    /* Compute final deferred shaded pixel color */
    Out.Color.rgb   = DiffuseLight + SpecularLight;
    Out.Color.a     = 1.0;
	
	#ifdef BLOOM_FILTER
    Out.Specular.rgb    = SpecularLight;
    Out.Specular.a      = 1.0;
    #endif

    #ifdef DEBUG_GBUFFER
	
	#   ifdef DEBUG_GBUFFER_WORLDPOS
	WorldPos.xyz += 0.01;
    Out.Color.rgb = WorldPos.xyz - floor(WorldPos.xyz);
	#   else
    Out.Color.rgb = float3(NormalAndDepthDist.a - floor(NormalAndDepthDist.a));
	#   endif
	
    #endif

    return Out;
}
