/*
 * D3D11 default shader file
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

/*
 * Global members
 */

Texture2D Texture2D0 : register(t0);
Texture2D Texture2D1 : register(t1);
Texture2D Texture2D2 : register(t2);
Texture2D Texture2D3 : register(t3);

SamplerState SamplerLinear0 : register(s0);
SamplerState SamplerLinear1	: register(s1);
SamplerState SamplerLinear2	: register(s2);
SamplerState SamplerLinear3	: register(s3);


/*
 * Constant buffer structures
 */

struct SLight
{
	int Model;							// Light model (Directionl, Point, Spot)
	int Enabled;						// Enabled/ disabled
	int2 Pad0;
	float4 Position;					// Position for Point- and Spot light and Direction for Directional light
	float4 Diffuse, Ambient, Specular;	// Light colors
	float4 SpotDir;						// Spot light direction
    float3 Attn;			            // Attunation values
	int Pad1;
	float Theta, Phi, Falloff, Range;	// Spot light attributes
};

struct SMaterial
{
	float4 Diffuse, Ambient, Specular, Emission;	// Material colors
	int Shading;									// Shading (flat, gouraud, phong, perpixel)
	int LightingEnabled;							// Global lighting enabled/ disabled
	int FogEnabled;									// Global fog enabled/ disabled
	float Shininess;								// Specular shininess
	int AlphaMethod;								// Alpha test function
	float AlphaReference;							// Alpha test reference value
	int2 Pad0;
};

struct STextureLayer
{
	int3 MapGenType;	// Texture coordinate generation
	int EnvType;		// Texture environment
	float4x4 Matrix;	// Texture coordiante transformation
};

struct SClipPlane
{
	int Enabled;	// Enabled/ disabled
	int3 Pad0;
	float3 Normal;	// Plane normal vector
	float Distance;	// Plane distance to the origin
};

struct SFogStates
{
	int Mode;			// Fog mode (Plane, Thick etc.)
	float Density;		// Density/ thickness
	float Near, Far;	// Near/ far planes
	float4 Color;		// Fog color
};


/*
 * Constant buffers
 */

cbuffer ConstantBufferLights : register(b0)
{
	SLight Lights[8];	// Light sources
};

cbuffer ConstantBufferObject : register(b1)
{
	float4x4 WorldMatrix, ViewMatrix, ProjectionMatrix;	// Matrices
	SMaterial Material;									// Material attributes
};

cbuffer ConstantBufferSurface : register(b2)
{
	uint TextureLayersEnabled;		// Bit mask for enabled texture layers
	int3 Pad0;
	STextureLayer TextureLayers[4];	// Texture surfaces
};

cbuffer ConstantBufferDriverSettings : register(b3)
{
	SClipPlane Planes[8];	// Clipping planes
	SFogStates Fog;			// Fog effect states
};


/*
 * Macros
 */

#define EPSILON                 0.001

#define MAPGEN_DISABLE 			0
#define MAPGEN_OBJECT_LINEAR	1
#define MAPGEN_EYE_LINEAR		2
#define MAPGEN_SPHERE_MAP		3
#define MAPGEN_NORMAL_MAP		4
#define MAPGEN_REFLECTION_MAP	5

#define LIGHT_DIRECTIONAL		0
#define LIGHT_POINT				1
#define LIGHT_SPOT				2

#define SHADING_FLAT			0
#define SHADING_GOURAUD			1
#define SHADING_PHONG			2
#define SHADING_PERPIXEL		3

#define TEXENV_MODULATE			0
#define TEXENV_REPLACE			1
#define TEXENV_ADD				2
#define TEXENV_ADDSIGNED		3
#define TEXENV_SUBTRACT			4
#define TEXENV_INTERPOLATE		5
#define TEXENV_DOT3				6

#define CMPSIZE_NEVER			0
#define CMPSIZE_EQUAL			1
#define CMPSIZE_NOTEQUAL		2
#define CMPSIZE_LESS			3
#define CMPSIZE_LESSEQUAL		4
#define CMPSIZE_GREATER			5
#define CMPSIZE_GREATEREQUAL	6
#define CMPSIZE_ALWAYS			7

#define FOG_STATIC_PALE			0
#define FOG_STATIC_THICK		1
#define FOG_VOLUMETRIC			2

#define TEXLAYER_ENABLED(i) (((TextureLayersEnabled >> i) & 0x00000001) != 0)


/*
 * Structures
 */

struct VertexInput
{
	float3 Position		: POSITION;
	float3 Normal		: NORMAL;
	float4 Color		: COLOR;
	float2 TexCoord0	: TEXCOORD0;
	float2 TexCoord1	: TEXCOORD1;
	float2 TexCoord2	: TEXCOORD2;
	float2 TexCoord3	: TEXCOORD3;
};

struct VertexPixelExchange
{
	float4 Position		: SV_Position;
	float3 Normal		: NORMAL;
	float4 Diffuse		: COLOR0;
	float4 Ambient		: COLOR1;
	float2 TexCoord0	: TEXCOORD0;
	float2 TexCoord1	: TEXCOORD1;
	float2 TexCoord2	: TEXCOORD2;
	float2 TexCoord3	: TEXCOORD3;
	
	float4 WorldPos		: POSITION1;
	float4 WorldViewPos	: POSITION2;
};


/*
 * Functions
 */

void LightCalculation(int i, float3 Normal, float3 Position, inout float4 ColorOut)
{
	float Intensity = 1.0;
	
	// Compute intensity
	switch (Lights[i].Model)
	{
		case LIGHT_DIRECTIONAL:
			Intensity = saturate(dot(Normal, Lights[i].Position.xyz));
			break;
		case LIGHT_POINT:
			Intensity = saturate(dot(Normal, normalize(Lights[i].Position.xyz - Position)));
			break;
		case LIGHT_SPOT:
			break;
	}
	
	// Compute attenuation
	if (Lights[i].Model != LIGHT_DIRECTIONAL)
	{
		const float Distance = distance(Lights[i].Position.xyz, Position);
		float Attn = 1.0 / ( Lights[i].Attn.x + Lights[i].Attn.y*Distance + Lights[i].Attn.z*Distance*Distance );
		//Intensity /= dot(Lights[i].Attn, float3(1.0, Distance, Distance*Distance));
        Intensity *= Attn;
	}
	
	// Apply light color
	ColorOut += Lights[i].Diffuse * Intensity;
	//ColorOut += Lights[i].Specular * pow(Intensity, Material.Shininess);
}

void FogCalculation(float Depth, inout float4 Color)
{
	float Factor = 0.0;
	
	switch (Fog.Mode)
	{
		case FOG_STATIC_PALE:
			Factor = exp(-Fog.Density * Depth); break;
		case FOG_STATIC_THICK:
		{
			float TempFactor = Fog.Density * Depth;
			Factor = exp(-TempFactor * TempFactor);
		}
		break;
	}
	
	clamp(Factor, 0.0, 1.0);
	
	Color.xyz = Fog.Color.xyz * (1.0 - Factor) + Color.xyz * Factor;
}

float ClippingPlane(int i, float4 Position)
{
	return dot(Position.xyz, normalize(Planes[i].Normal)) + Planes[i].Distance;
}

void TexCoordGeneration(int MapGenType, float Pos, float WorldViewPos, float TransNormal, float TexCoordIn, inout float TexCoordOut)
{
	// Texture coordinate generation
	switch (MapGenType)
	{
		case MAPGEN_DISABLE:
			TexCoordOut = TexCoordIn; break;
		case MAPGEN_OBJECT_LINEAR:
			TexCoordOut = Pos; break;
		case MAPGEN_EYE_LINEAR:
			TexCoordOut = WorldViewPos; break;
		case MAPGEN_SPHERE_MAP:
			TexCoordOut = TransNormal*0.5 + 0.5; break;
		case MAPGEN_NORMAL_MAP:
			break; //!TODO!
		case MAPGEN_REFLECTION_MAP:
			break; //!TODO!
	}
}

void TextureMapping(int i, Texture2D Tex, SamplerState Sampler, float2 TexCoord, inout float4 ColorOut)
{
	const float4 TexColor = Tex.Sample(Sampler, TexCoord);
	
	switch (TextureLayers[i].EnvType)
	{
		case TEXENV_MODULATE:
			ColorOut *= TexColor; break;
		case TEXENV_REPLACE:
			ColorOut = TexColor; break;
		case TEXENV_ADD:
			ColorOut += TexColor; break;
		case TEXENV_ADDSIGNED:
			ColorOut += TexColor - float4(0.5, 0.5, 0.5, 1.0); break;
		case TEXENV_SUBTRACT:
			ColorOut -= TexColor; break;
		case TEXENV_INTERPOLATE:
			break; //!TODO!
		case TEXENV_DOT3:
			ColorOut.r = (ColorOut.r - 0.5)*(TexColor.r - 0.5);
			ColorOut.g = (ColorOut.g - 0.5)*(TexColor.g - 0.5);
			ColorOut.b = (ColorOut.b - 0.5)*(TexColor.b - 0.5);
			break;
	}
}


/*
 * ======= Vertex Shader =======
 */

VertexPixelExchange VertexMain(VertexInput In)
{
	#define mcrTexCoordGeneration(t, c)																							\
		TexCoordGeneration(TextureLayers[t].MapGenType.x, In.Position.x, Out.WorldViewPos.x, TransNormal.x, In.c.x, Out.c.x);	\
		TexCoordGeneration(TextureLayers[t].MapGenType.y, In.Position.y, Out.WorldViewPos.y, TransNormal.y, In.c.y, Out.c.y);
	#define mcrTexCoordTransform(i, t) \
		Out.t = mul(float4(Out.t.x, Out.t.y, 0.0, 1.0), TextureLayers[i].Matrix).xy;
	
	// Temporary variables
	VertexPixelExchange Out = (VertexPixelExchange)0;
	
	// Compute vertex positions (local, gloabl, projected)
	Out.WorldPos		= mul(WorldMatrix, float4(In.Position, 1.0));
	Out.WorldViewPos	= mul(ViewMatrix, Out.WorldPos);
	Out.Position		= mul(ProjectionMatrix, Out.WorldViewPos);
	
	// Compute normals
	float3 TransNormal	= mul((float3x3)WorldMatrix, In.Normal);
	TransNormal			= mul((float3x3)ViewMatrix, TransNormal);
	TransNormal			= normalize(TransNormal);
	Out.Normal			= TransNormal;
	
	// Compute final vertex color
	Out.Diffuse = In.Color * Material.Diffuse;
	Out.Ambient = Material.Ambient;
	
	// Light computations
	if (Material.LightingEnabled && Material.Shading < SHADING_PHONG)
	{
		float4 LightColor = Out.Ambient;
		
		for (int i = 0; i < 8; ++i)
		{
			if (Lights[i].Enabled)
				LightCalculation(i, Out.Normal, Out.WorldViewPos.xyz, LightColor);
		}
		
		LightColor = saturate(LightColor);
		
		Out.Diffuse.rgb *= LightColor.rgb;
		Out.Diffuse = saturate(Out.Diffuse);
	}
	
	// Compute texture coordinates
	if (TEXLAYER_ENABLED(0))
	{
		mcrTexCoordGeneration(0, TexCoord0);
		mcrTexCoordTransform(0, TexCoord0);
	}
	if (TEXLAYER_ENABLED(1))
	{
		mcrTexCoordGeneration(1, TexCoord1);
		mcrTexCoordTransform(1, TexCoord1);
	}
	if (TEXLAYER_ENABLED(2))
	{
		mcrTexCoordGeneration(2, TexCoord2);
		mcrTexCoordTransform(2, TexCoord2);
	}
	if (TEXLAYER_ENABLED(3))
	{
		mcrTexCoordGeneration(3, TexCoord3);
		mcrTexCoordTransform(3, TexCoord3);
	}
	
	return Out;
	
	#undef mcrTexCoordGeneration
	#undef mcrTexCoordTransform
}


/*
 * ======= Pixel Shader =======
 */

float4 PixelMain(VertexPixelExchange In) : SV_Target
{
	// Temporary variables
	float4 Out = (float4)0;
	
	float4 TexColor		= 1.0;
	float4 LightColor	= 1.0;
	
	// Light computations
	if (Material.LightingEnabled && Material.Shading >= SHADING_PHONG)
	{
		LightColor = In.Ambient;
		float3 Normal = normalize(In.Normal);
		
		for (int i = 0; i < 8; ++i)
		{
			if (Lights[i].Enabled)
				LightCalculation(i, Normal, In.WorldViewPos.xyz, LightColor);
		}
		
		LightColor = saturate(LightColor);
	}
	
	// Texture mapping
	if (TEXLAYER_ENABLED(0))
		TextureMapping(0, Texture2D0, SamplerLinear0, In.TexCoord0, TexColor);
	if (TEXLAYER_ENABLED(1))
		TextureMapping(1, Texture2D1, SamplerLinear1, In.TexCoord1, TexColor);
	if (TEXLAYER_ENABLED(2))
		TextureMapping(2, Texture2D2, SamplerLinear2, In.TexCoord2, TexColor);
	if (TEXLAYER_ENABLED(3))
		TextureMapping(3, Texture2D3, SamplerLinear3, In.TexCoord3, TexColor);
	
	// Final color output
	Out = In.Diffuse * TexColor * float4(LightColor.rgb, 1.0);
	
	// Fog computations
	if (Material.FogEnabled)
		FogCalculation(In.WorldViewPos.z, Out);
	
	// Alpha reference method
	switch (Material.AlphaMethod)
	{
		case CMPSIZE_ALWAYS:
			break;
		case CMPSIZE_GREATEREQUAL:
            clip(Out.a - Material.AlphaReference);
			break;
		case CMPSIZE_GREATER:
            clip(Out.a - Material.AlphaReference - EPSILON);
			break;
		case CMPSIZE_LESSEQUAL:
            clip(Material.AlphaReference - Out.a);
			break;
		case CMPSIZE_LESS:
            clip(Material.AlphaReference - Out.a - EPSILON);
			break;
		case CMPSIZE_NOTEQUAL:
            clip(abs(Out.a - Material.AlphaReference) - EPSILON);
			break;
		case CMPSIZE_EQUAL:
            clip(-abs(Out.a - Material.AlphaReference) + EPSILON);
			break;
		case CMPSIZE_NEVER:
			clip(-1.0);
            break;
		default:
			break;
	}
	
	// Process clipping planes
	for (int i = 0; i < 8; ++i)
	{
		if (Planes[i].Enabled)
			clip(ClippingPlane(i, In.WorldPos));
	}
	
	// Return the output color
	return Out;
}

