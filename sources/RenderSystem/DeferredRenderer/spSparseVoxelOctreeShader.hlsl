/*
 * Sparse voxel octree D3D11 shader file
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

#include <softpixelengine>

#include "spSparseVoxelOctreeShaderProcs.shader"

/*
 * ======= Vertex shader: =======
 */

/* === Structures === */

struct SVertexInput
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};


/* === Uniforms === */

cbuffer BufferMain : register(b0)
{
	float4x4 ProjectionMatrices[6];
    float4 BoundBoxMin;
    float4 BoundBoxMax;
	float4 VolumeSize;
};

cbuffer BufferObject : register(b1)
{
	float4x4 WorldMatrix;
};


/* === Functions === */

SVertexInput VertexMain(SVertexInput In)
{
	SVertexInput Out = (SVertexInput)0;
	
	/* Process vertex transformation */
	Out.Position = mul(WorldMatrix, float4(In.Position, 1.0)).xyz;
	Out.TexCoord = In.TexCoord;
	
	return Out;
}


/*
 * ======= Geometry shader: =======
 */

/* === Structures === */

struct SGeometryOutput
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};


/* === Functions === */

[maxvertexcount(3)]
void GeometryMain(triangle SVertexInput In[3], inout TriangleStream<SGeometryOutput> OutStream)
{
	/* Find dominant axis by triangle normal */
	float3 Normal = cross(
		In[1].Position - In[0].Position,
		In[2].Position - In[0].Position
	);
	
	int DominantAxis = GetDominantAxis(Normal);
	
	/* Construct triangle for rasterizer */
	SGeometryOutput Out = (SGeometryOutput)0;
	
	for (uint i = 0; i < 3; ++i)
	{
		float3 WorldPos = ((In[i].Position - BoundBoxMin.xyz) / (BoundBoxMax.xyz - BoundBoxMin.xyz)) * VolumeSize.xyz;
		
		Out.Position = mul(ProjectionMatrices[DominantAxis], float4(WorldPos, 1.0));
		Out.TexCoord = In[i].TexCoord;
		Out.WorldPos = WorldPos;
		
		OutStream.Append(Out);
	}
	
	OutStream.RestartStrip();
}


/*
 * ======= Pixel shader: =======
 */

SAMPLER2D(DiffuseMap, 0);

//! Output voxel cube texture.
RWTexture3D<float3> OutVoxelTex : register(u0);

void PixelMain(SGeometryOutput In)
{
	/* Get diffuse color */
	float3 Color = tex2D(DiffuseMap, In.TexCoord);
	
	/* Store color into cube texture */
	uint3 Index = (uint3)In.WorldPos;
	
	OutVoxelTex[Index] = Color;
}