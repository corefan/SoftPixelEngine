/*
 * Direct3D9 render system file
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

#include "RenderSystem/Direct3D9/spDirect3D9RenderSystem.hpp"

#if defined(SP_COMPILE_WITH_DIRECT3D9)


#include "Base/spInternalDeclarations.hpp"
#include "Base/spSharedObjects.hpp"
#include "Base/spBaseExceptions.hpp"
#include "SceneGraph/spSceneCamera.hpp"
#include "Platform/spSoftPixelDeviceOS.hpp"
#include "Framework/Cg/spCgShaderProgramD3D9.hpp"
#include "RenderSystem/Direct3D9/spDirect3D9VertexBuffer.hpp"
#include "RenderSystem/Direct3D9/spDirect3D9IndexBuffer.hpp"
#include "RenderSystem/Direct3D9/spDirect3D9Query.hpp"

#include <boost/foreach.hpp>


namespace sp
{

extern SoftPixelDevice* GlbEngineDev;
extern scene::SceneGraph* GlbSceneGraph;

namespace video
{


/*
 * Internal macros
 */

#define D3D_MATRIX(m) (D3DMATRIX*)((void*)&(m))
#define D3D_VECTOR(v) (D3DVECTOR*)((void*)&(v))


/*
 * Internal members
 */

const io::stringc d3dDllFileName = "d3dx9_" + io::stringc(static_cast<s32>(D3DX_SDK_VERSION)) + ".dll";

const s32 D3DCompareList[] =
{
    D3DCMP_NEVER, D3DCMP_EQUAL, D3DCMP_NOTEQUAL, D3DCMP_LESS, D3DCMP_LESSEQUAL,
    D3DCMP_GREATER, D3DCMP_GREATEREQUAL, D3DCMP_ALWAYS,
};

const s32 D3DMappingGenList[] =
{
    D3DTSS_TCI_PASSTHRU, D3DTSS_TCI_PASSTHRU, D3DTSS_TCI_CAMERASPACEPOSITION, D3DTSS_TCI_SPHEREMAP,
    D3DTSS_TCI_CAMERASPACENORMAL, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR,
};

const s32 D3DTextureEnvList[] =
{
    D3DTOP_MODULATE, D3DTOP_SELECTARG1, D3DTOP_ADD, D3DTOP_ADDSIGNED,
    D3DTOP_SUBTRACT, D3DTOP_LERP, D3DTOP_DOTPRODUCT3,
};

const s32 D3DBlendingList[] =
{
    D3DBLEND_ZERO, D3DBLEND_ONE, D3DBLEND_SRCCOLOR, D3DBLEND_INVSRCCOLOR, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA,
    D3DBLEND_DESTCOLOR, D3DBLEND_INVDESTCOLOR, D3DBLEND_DESTALPHA, D3DBLEND_INVDESTALPHA,
};


const D3DSTENCILOP D3DStencilOperationList[] =
{
    D3DSTENCILOP_KEEP, D3DSTENCILOP_ZERO, D3DSTENCILOP_REPLACE, D3DSTENCILOP_INCRSAT,
    D3DSTENCILOP_INCR, D3DSTENCILOP_DECRSAT, D3DSTENCILOP_DECR, D3DSTENCILOP_INVERT,
};


/*
 * Direct3D9RenderSystem class
 */

Direct3D9RenderSystem::Direct3D9RenderSystem() :
    RenderSystem                (RENDERER_DIRECT3D9 ),
    D3DInstance_                (0                  ),
    D3DDevice_                  (0                  ),
    D3DDefVertexBuffer_         (0                  ),
    D3DDefFlexibleVertexBuffer_ (0                  ),
    PrevRenderTargetSurface_    (0                  ),
    CurD3DTexture_              (0                  ),
    CurD3DCubeTexture_          (0                  ),
    CurD3DVolumeTexture_        (0                  ),
    ClearColor_                 (video::color::empty),
    ClearColorMask_             (1, 1, 1, 1         ),
    ClearStencil_               (0                  )
{
    /* Create the Direct3D renderer */
    D3DInstance_ = Direct3DCreate9(D3D_SDK_VERSION);
    
    if (!D3DInstance_)
        throw io::DefaultException("Could not create Direct3D9 interface");
}
Direct3D9RenderSystem::~Direct3D9RenderSystem()
{
    /* Release all Direct3D9 fonts */
    foreach (Font* FontObj, FontList_)
        releaseFontObject(FontObj);
    
    /* Close and release the standard- & flexible vertex buffer */
    Direct3D9RenderSystem::releaseObject(D3DDefVertexBuffer_);
    Direct3D9RenderSystem::releaseObject(D3DDefFlexibleVertexBuffer_);
    
    /* Close and release Direct3D */
    Direct3D9RenderSystem::releaseObject(D3DInstance_);
}


/*
 * ======= Initialization functions =======
 */

void Direct3D9RenderSystem::setupConfiguration()
{
    /* Get all device capabilities */
    D3DDevice_->GetDeviceCaps(&DevCaps_);
    
    MaxClippingPlanes_ = DevCaps_.MaxUserClipPlanes;
    
    /*
     * Create the standard vertex buffer
     * used for 2d drawing operations
     * (drawing: rectangle, images etc.)
     */
    D3DDevice_->CreateVertexBuffer(
        sizeof(SPrimitiveVertex)*4,
        0,
        FVF_VERTEX2D,
        D3DPOOL_DEFAULT,
        &D3DDefVertexBuffer_,
        0
    );
    
    if (!D3DDefVertexBuffer_)
    {
        io::Log::error("Could not create Direct3D9 vertex buffer");
        return;
    }
    
    /*
     * Create the flexible vertex buffer
     * used for 2d drawing operations
     * (drawing: polygon & other objects with undefined sizes)
     */
    D3DDevice_->CreateVertexBuffer(
        sizeof(SPrimitiveVertex),
        0,
        FVF_VERTEX2D,
        D3DPOOL_DEFAULT,
        &D3DDefFlexibleVertexBuffer_,
        0
    );
    
    if (!D3DDefFlexibleVertexBuffer_)
    {
        io::Log::error("Could not create Direct3D9 vertex buffer");
        return;
    }
    
    /* Default settings */
    D3DDevice_->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    D3DDevice_->SetRenderState(D3DRS_ALPHATESTENABLE, true);
    D3DDevice_->SetRenderState(D3DRS_SPECULARENABLE, true);
    D3DDevice_->SetRenderState(D3DRS_NORMALIZENORMALS, true);
    
    /* Default queries */
    RenderQuery_[RENDERQUERY_SHADER             ] = queryVideoSupport(VIDEOSUPPORT_SHADER               );
    RenderQuery_[RENDERQUERY_MULTI_TEXTURE      ] = queryVideoSupport(VIDEOSUPPORT_MULTI_TEXTURE        );
    RenderQuery_[RENDERQUERY_HARDWARE_MESHBUFFER] = queryVideoSupport(VIDEOSUPPORT_HARDWARE_MESHBUFFER  );
    RenderQuery_[RENDERQUERY_RENDERTARGET       ] = queryVideoSupport(VIDEOSUPPORT_RENDERTARGET         );
}


/*
 * ======= Renderer information =======
 */

io::stringc Direct3D9RenderSystem::getRenderer() const
{
    D3DADAPTER_IDENTIFIER9 Adapter;
    D3DInstance_->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &Adapter);
    return io::stringc(Adapter.Description);
}
io::stringc Direct3D9RenderSystem::getVersion() const
{
    return queryVideoSupport(VIDEOSUPPORT_HLSL_3_0) ? "Direct3D 9.0c" : "Direct3D 9.0";
}
io::stringc Direct3D9RenderSystem::getVendor() const
{
    D3DADAPTER_IDENTIFIER9 Adapter;
    D3DInstance_->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &Adapter);
    return getVendorNameByID((u32)Adapter.VendorId);
}
io::stringc Direct3D9RenderSystem::getShaderVersion() const
{
    return queryVideoSupport(VIDEOSUPPORT_HLSL_3_0) ? "HLSL Shader Model 3.0" : "HLSL Shader Model 2.0";
}

bool Direct3D9RenderSystem::queryVideoSupport(const EVideoFeatureSupport Query) const
{
    switch (Query)
    {
        case VIDEOSUPPORT_ANTIALIASING:
            return true; // (todo)
        case VIDEOSUPPORT_MULTI_TEXTURE:
            return getMultitexCount() > 1;
        case VIDEOSUPPORT_HARDWARE_MESHBUFFER:
            return true;
        case VIDEOSUPPORT_STENCIL_BUFFER:
            return DevCaps_.StencilCaps != 0;
        case VIDEOSUPPORT_RENDERTARGET:
        case VIDEOSUPPORT_MULTISAMPLE_RENDERTARGET:
        case VIDEOSUPPORT_QUERIES:
            return true;
            
        case VIDEOSUPPORT_BILINEAR_FILTER:
            return (DevCaps_.TextureFilterCaps & D3DPTFILTERCAPS_MINFPOINT) != 0;
        case VIDEOSUPPORT_TRILINEAR_FILTER:
            return (DevCaps_.TextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) != 0;
        case VIDEOSUPPORT_ANISOTROPY_FILTER:
            return (DevCaps_.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0;
        case VIDEOSUPPORT_MIPMAPS:
            return (DevCaps_.TextureCaps & D3DPTEXTURECAPS_MIPMAP) != 0;
        case VIDEOSUPPORT_VOLUMETRIC_TEXTURE:
            return (DevCaps_.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP) != 0;
            
        case VIDEOSUPPORT_VETEX_PROGRAM:
        case VIDEOSUPPORT_FRAGMENT_PROGRAM:
            return true; // (todo)
        case VIDEOSUPPORT_SHADER:
        case VIDEOSUPPORT_HLSL:
        case VIDEOSUPPORT_HLSL_1_1:
            return DevCaps_.VertexShaderVersion >= D3DVS_VERSION(1, 1) && DevCaps_.PixelShaderVersion >= D3DPS_VERSION(1, 1);
        case VIDEOSUPPORT_HLSL_2_0:
            return DevCaps_.VertexShaderVersion >= D3DVS_VERSION(2, 0) && DevCaps_.PixelShaderVersion >= D3DPS_VERSION(2, 0);
        case VIDEOSUPPORT_HLSL_3_0:
            return DevCaps_.VertexShaderVersion >= D3DVS_VERSION(3, 0) && DevCaps_.PixelShaderVersion >= D3DPS_VERSION(3, 0);
    }
    
    return false;
}

s32 Direct3D9RenderSystem::getMultitexCount() const
{
    return DevCaps_.MaxTextureBlendStages;
}
s32 Direct3D9RenderSystem::getMaxAnisotropicFilter() const
{
    return DevCaps_.MaxAnisotropy;
}
s32 Direct3D9RenderSystem::getMaxLightCount() const
{
    return DevCaps_.MaxActiveLights;
}


/*
 * ======= User controll functions (clear buffers, flip buffers, 2d drawing etc.) =======
 */

void Direct3D9RenderSystem::clearBuffers(const s32 ClearFlags)
{
    setViewport(0, dim::size2di(gSharedObjects.ScreenWidth, gSharedObjects.ScreenHeight));
    
    const video::color ClearColor(ClearColor_ * ClearColorMask_);
    
    DWORD Mask = 0;
    
    if (ClearFlags & BUFFER_COLOR)
        Mask |= D3DCLEAR_TARGET;
    if (ClearFlags & BUFFER_DEPTH)
        Mask |= D3DCLEAR_ZBUFFER;
    if (ClearFlags & BUFFER_STENCIL)
        Mask |= D3DCLEAR_STENCIL;
    
    D3DDevice_->Clear(0, 0, Mask, ClearColor.getSingle(), 1.0f, ClearStencil_);
}


/*
 * ======= Setting-/ getting functions =======
 */

void Direct3D9RenderSystem::setShadeMode(const EShadeModeTypes ShadeMode)
{
    switch (ShadeMode)
    {
        case SHADEMODE_SMOOTH:
            D3DDevice_->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
            break;
        case SHADEMODE_FLAT:
            D3DDevice_->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
            break;
    }
}

void Direct3D9RenderSystem::setClearColor(const color &Color)
{
    ClearColor_ = Color;
}

void Direct3D9RenderSystem::setColorMask(bool isRed, bool isGreen, bool isBlue, bool isAlpha)
{
    DWORD Mask = 0;
    ClearColorMask_ = video::color::empty;
    
    if (isRed)
    {
        Mask |= D3DCOLORWRITEENABLE_RED;
        ClearColorMask_.Red = 1;
    }
    if (isGreen)
    {
        Mask |= D3DCOLORWRITEENABLE_GREEN;
        ClearColorMask_.Green = 1;
    }
    if (isBlue)
    {
        Mask |= D3DCOLORWRITEENABLE_BLUE;
        ClearColorMask_.Blue = 1;
    }
    if (isAlpha)
    {
        Mask |= D3DCOLORWRITEENABLE_ALPHA;
        ClearColorMask_.Alpha = 1;
    }
    
    D3DDevice_->SetRenderState(D3DRS_COLORWRITEENABLE, Mask);
}

void Direct3D9RenderSystem::setDepthMask(bool isDepth)
{
    D3DDevice_->SetRenderState(D3DRS_ZWRITEENABLE, isDepth);
}

void Direct3D9RenderSystem::setAntiAlias(bool isAntiAlias)
{
    D3DDevice_->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, isAntiAlias);
}

void Direct3D9RenderSystem::setDepthRange(f32 Near, f32 Far)
{
    RenderSystem::setDepthRange(Near, Far);
    
    /* Setup only depth range for viewport */
    D3DVIEWPORT9 Viewport;
    D3DDevice_->GetViewport(&Viewport);
    {
        Viewport.MinZ = DepthRange_.Near;
        Viewport.MaxZ = DepthRange_.Far;
    }
    D3DDevice_->SetViewport(&Viewport);
}

void Direct3D9RenderSystem::setDepthClip(bool Enable)
{
    RenderSystem::setDepthClip(Enable);
    D3DDevice_->SetRenderState(D3DRS_CLIPPING, Enable);
}


/*
 * ======= Stencil buffer =======
 */

void Direct3D9RenderSystem::setStencilMask(u32 BitMask)
{
    D3DDevice_->SetRenderState(D3DRS_STENCILMASK, BitMask);
}

void Direct3D9RenderSystem::setStencilMethod(const ESizeComparisionTypes Method, s32 Reference, u32 BitMask)
{
    D3DDevice_->SetRenderState(D3DRS_STENCILFUNC, D3DCompareList[Method]);
    D3DDevice_->SetRenderState(D3DRS_STENCILREF, Reference);
    D3DDevice_->SetRenderState(D3DRS_STENCILWRITEMASK, BitMask);
}

void Direct3D9RenderSystem::setStencilOperation(const EStencilOperations FailOp, const EStencilOperations ZFailOp, const EStencilOperations ZPassOp)
{
    D3DDevice_->SetRenderState(D3DRS_STENCILZFAIL, D3DStencilOperationList[FailOp]);
    D3DDevice_->SetRenderState(D3DRS_STENCILFAIL, D3DStencilOperationList[ZFailOp]);
    D3DDevice_->SetRenderState(D3DRS_STENCILPASS, D3DStencilOperationList[ZPassOp]);
}

void Direct3D9RenderSystem::setClearStencil(s32 Stencil)
{
    ClearStencil_ = Stencil;
}


/*
 * ======= Rendering functions =======
 */

bool Direct3D9RenderSystem::setupMaterialStates(const MaterialStates* Material, bool Forced)
{
    /* Check for equality to optimize render path */
    if ( GlobalMaterialStates_ != 0 || !Material || ( !Forced && ( PrevMaterial_ == Material || Material->compare(PrevMaterial_) ) ) )
        return false;
    
    PrevMaterial_ = Material;
    
    /* Cull facing */
    switch (Material->getRenderFace())
    {
        case video::FACE_FRONT:
            D3DDevice_->SetRenderState(D3DRS_CULLMODE, isFrontFace_ ? D3DCULL_CCW : D3DCULL_CW);
            break;
        case video::FACE_BACK:
            D3DDevice_->SetRenderState(D3DRS_CULLMODE, isFrontFace_ ? D3DCULL_CW : D3DCULL_CCW);
            break;
        case video::FACE_BOTH:
            D3DDevice_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            break;
    }
    
    /* Fog effect */
    D3DDevice_->SetRenderState(D3DRS_FOGENABLE, __isFog && Material->getFog());
    
    /* Color material */
    D3DDevice_->SetRenderState(D3DRS_COLORVERTEX, Material->getColorMaterial());
    
    /* Lighting material */
    if (__isLighting && Material->getLighting())
    {
        D3DMATERIAL9 D3DMat;
        
        D3DDevice_->SetRenderState(D3DRS_LIGHTING, true);
        
        /* Diffuse, ambient, specular and emissive color */
        D3DMat.Diffuse = getD3DColor(Material->getDiffuseColor());
        D3DMat.Ambient = getD3DColor(Material->getAmbientColor());
        D3DMat.Specular = getD3DColor(Material->getSpecularColor());
        D3DMat.Emissive = getD3DColor(Material->getEmissionColor());
        
        /* Shininess */
        D3DMat.Power = Material->getShininessFactor();
        
        /* Set the material */
        D3DDevice_->SetMaterial(&D3DMat);
    }
    else
        D3DDevice_->SetRenderState(D3DRS_LIGHTING, false);
    
    /* Depth functions */
    if (Material->getDepthBuffer())
    {
        D3DDevice_->SetRenderState(D3DRS_ZENABLE, true);
        D3DDevice_->SetRenderState(D3DRS_ZFUNC, D3DCompareList[Material->getDepthMethod()]);
    }
    else
        D3DDevice_->SetRenderState(D3DRS_ZENABLE, false);
    
    /* Blending mode */
    if (Material->getBlending())
    {
        D3DDevice_->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
        D3DDevice_->SetRenderState(D3DRS_SRCBLEND, D3DBlendingList[Material->getBlendSource()]);
        D3DDevice_->SetRenderState(D3DRS_DESTBLEND, D3DBlendingList[Material->getBlendTarget()]);
    }
    else
        D3DDevice_->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
    
    /* Polygon offset */
    if (Material->getPolygonOffset())
    {
        D3DDevice_->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD*)&Material->OffsetFactor_);
        D3DDevice_->SetRenderState(D3DRS_DEPTHBIAS, *(DWORD*)&Material->OffsetUnits_);
    }
    else
    {
        D3DDevice_->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
        D3DDevice_->SetRenderState(D3DRS_DEPTHBIAS, 0);
    }
    
    /* Alpha functions */
    D3DDevice_->SetRenderState(D3DRS_ALPHAFUNC, D3DCompareList[Material->getAlphaMethod()]);
    D3DDevice_->SetRenderState(D3DRS_ALPHAREF, s32(Material->getAlphaReference() * 255));
    
    /* Polygon mode */
    D3DDevice_->SetRenderState(D3DRS_FILLMODE, D3DFILL_POINT + Material->getWireframeFront());
    
    /* Flexible vertex format (FVF) */
    D3DDevice_->SetFVF(FVF_VERTEX3D);
    
    #ifdef SP_COMPILE_WITH_RENDERSYS_QUERIES
    ++RenderSystem::NumMaterialUpdates_;
    #endif
    
    return true;
}

void Direct3D9RenderSystem::setupTextureLayer(
    u8 LayerIndex, const dim::matrix4f &TexMatrix, const ETextureEnvTypes EnvType,
    const EMappingGenTypes GenType, s32 MappingCoordsFlags)
{
    /* Setup texture matrix */
    D3DDevice_->SetTransform(
        static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + LayerIndex), D3D_MATRIX(TexMatrix)
    );
    D3DDevice_->SetTextureStageState(LayerIndex, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
    
    /* Texture coordinate generation */
    D3DDevice_->SetTextureStageState(
        LayerIndex, D3DTSS_TEXCOORDINDEX,
        MappingCoordsFlags != MAPGEN_NONE ? D3DMappingGenList[GenType] : LayerIndex
    );
    
    /* Texture stage states */
    D3DDevice_->SetTextureStageState(LayerIndex, D3DTSS_COLOROP, D3DTextureEnvList[EnvType]);
    D3DDevice_->SetTextureStageState(LayerIndex, D3DTSS_ALPHAOP, D3DTextureEnvList[EnvType]);
    
    /* Setup alpha blending material */
    if (LayerIndex == 0)
    {
        D3DDevice_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        D3DDevice_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    }
}

void Direct3D9RenderSystem::updateLight(
    u32 LightID, const scene::ELightModels LightType, bool IsVolumetric,
    const dim::vector3df &Direction, const scene::SLightCone &SpotCone, const scene::SLightAttenuation &Attn)
{
    if (LightID >= MAX_COUNT_OF_LIGHTS)
        return;
    
    /* Get the light source */
    D3DDevice_->GetLight(LightID, &D3DActiveLight_);
    
    /* Update type and direction */
    dim::vector3df LightDir(Direction);
    
    switch (LightType)
    {
        case scene::LIGHT_DIRECTIONAL:
            LightDir = scene::spWorldMatrix.getRotationMatrix() * LightDir;
            D3DActiveLight_.Type        = D3DLIGHT_DIRECTIONAL;
            D3DActiveLight_.Direction   = *D3D_VECTOR(LightDir);
            break;
        case scene::LIGHT_POINT:
            LightDir = scene::spWorldMatrix.getRotationMatrix() * -LightDir;
            D3DActiveLight_.Type        = D3DLIGHT_POINT;
            D3DActiveLight_.Direction   = *D3D_VECTOR(LightDir);
            break;
        case scene::LIGHT_SPOT:
            D3DActiveLight_.Type        = D3DLIGHT_SPOT;
            break;
    }
    
    /* Lighting location */
    const dim::vector3df WorldPos(scene::spWorldMatrix.getPosition());
    D3DActiveLight_.Position = *D3D_VECTOR(WorldPos);
    
    /* Spot light attributes */
    D3DActiveLight_.Theta   = SpotCone.InnerAngle * 2.0f * math::DEG;
    D3DActiveLight_.Phi     = SpotCone.OuterAngle * 2.0f * math::DEG;
    
    /* Volumetric light attenuations */
    if (IsVolumetric)
    {
        D3DActiveLight_.Attenuation0 = Attn.Constant;
        D3DActiveLight_.Attenuation1 = Attn.Linear;
        D3DActiveLight_.Attenuation2 = Attn.Quadratic;
    }
    else
    {
        D3DActiveLight_.Attenuation0 = 1.0f;
        D3DActiveLight_.Attenuation1 = 0.0f;
        D3DActiveLight_.Attenuation2 = 0.0f;
    }
    
    /* Set the light source */
    D3DDevice_->SetLight(LightID, &D3DActiveLight_);
}


/* === Hardware mesh buffers === */

void Direct3D9RenderSystem::createVertexBuffer(void* &BufferID)
{
    /* Create new hardware vertex buffer */
    BufferID = new D3D9VertexBuffer();
}

void Direct3D9RenderSystem::createIndexBuffer(void* &BufferID)
{
    /* Create new hardware index buffer */
    BufferID = new D3D9IndexBuffer();
}

void Direct3D9RenderSystem::deleteVertexBuffer(void* &BufferID)
{
    if (BufferID)
    {
        /* Remove from resource manager */
        ResMngr_.remove(ResMngr_.VertexBuffers, BufferID);

        /* Delete hardware vertex buffer */
        D3D9VertexBuffer* Buffer = reinterpret_cast<D3D9VertexBuffer*>(BufferID);
        delete Buffer;
        
        BufferID = 0;
    }
}

void Direct3D9RenderSystem::deleteIndexBuffer(void* &BufferID)
{
    if (BufferID)
    {
        /* Remove from resource manager */
        ResMngr_.remove(ResMngr_.IndexBuffers, BufferID);

        /* Delete hardware index buffer */
        D3D9IndexBuffer* Buffer = reinterpret_cast<D3D9IndexBuffer*>(BufferID);
        delete Buffer;
        
        BufferID = 0;
    }
}

void Direct3D9RenderSystem::updateVertexBuffer(
    void* BufferID, const dim::UniversalBuffer &BufferData, const VertexFormat* Format, const EHWBufferUsage Usage)
{
    if (BufferID && Format)
    {
        /* Update hardware vertex buffer */
        D3D9VertexBuffer* Buffer = reinterpret_cast<D3D9VertexBuffer*>(BufferID);
        Buffer->update(D3DDevice_, BufferData, Format, Usage);

        /* Store in resource manager */
        if (!ResMngr_.contains(ResMngr_.VertexBuffers, BufferID))
            ResMngr_.add(ResMngr_.VertexBuffers, BufferID, Buffer->HWBuffer_);
    }
}

void Direct3D9RenderSystem::updateIndexBuffer(
    void* BufferID, const dim::UniversalBuffer &BufferData, const IndexFormat* Format, const EHWBufferUsage Usage)
{
    if (BufferID && Format)
    {
        /* Update hardware index buffer */
        D3D9IndexBuffer* Buffer = reinterpret_cast<D3D9IndexBuffer*>(BufferID);
        Buffer->update(D3DDevice_, BufferData, Format, Usage);

        /* Store in resource manager */
        if (!ResMngr_.contains(ResMngr_.IndexBuffers, BufferID))
            ResMngr_.add(ResMngr_.IndexBuffers, BufferID, Buffer->HWBuffer_);
    }
}

void Direct3D9RenderSystem::updateVertexBufferElement(void* BufferID, const dim::UniversalBuffer &BufferData, u32 Index)
{
    if (BufferID && BufferData.getSize())
    {
        D3D9VertexBuffer* Buffer = reinterpret_cast<D3D9VertexBuffer*>(BufferID);
        Buffer->update(D3DDevice_, BufferData, Index);
    }
}

void Direct3D9RenderSystem::updateIndexBufferElement(void* BufferID, const dim::UniversalBuffer &BufferData, u32 Index)
{
    if (BufferID && BufferData.getSize())
    {
        D3D9IndexBuffer* Buffer = reinterpret_cast<D3D9IndexBuffer*>(BufferID);
        Buffer->update(D3DDevice_, BufferData, Index);
    }
}

bool Direct3D9RenderSystem::bindMeshBuffer(const MeshBuffer* MeshBuffer)
{
    /* Get reference mesh buffer */
    if (!MeshBuffer || !MeshBuffer->renderable())
        return false;
    
    /* Get hardware vertex- and index buffers */
    D3D9VertexBuffer* VertexBuffer = reinterpret_cast<D3D9VertexBuffer*>(MeshBuffer->getVertexBufferID());
    
    if (!VertexBuffer->HWBuffer_)
        return false;
    
    /* Bind textures */
    if (__isTexturing)
        bindTextureLayers(MeshBuffer->getTextureLayerList());
    else
        unbindPrevTextureLayers();
    
    /* Setup vertex format */
    D3DDevice_->SetFVF(VertexBuffer->FormatFlags_);
    
    /* Bind hardware vertex mesh buffer */
    D3DDevice_->SetStreamSource(
        0, VertexBuffer->HWBuffer_,
        0, MeshBuffer->getVertexFormat()->getFormatSize()
    );
    
    #ifdef SP_COMPILE_WITH_RENDERSYS_QUERIES
    ++RenderSystem::NumMeshBufferBindings_;
    #endif
    
    return true;
}

void Direct3D9RenderSystem::unbindMeshBuffer()
{
    /* Unbind hardware vertex mesh buffer */
    D3DDevice_->SetStreamSource(0, 0, 0, 0);
}

void Direct3D9RenderSystem::drawMeshBufferPart(const MeshBuffer* MeshBuffer, u32 StartOffset, u32 NumVertices)
{
    if (!MeshBuffer || NumVertices == 0 || StartOffset + NumVertices > MeshBuffer->getVertexCount())
        return;
    
    /* Surface shader callback */
    if (CurShaderClass_ && ShaderSurfaceCallback_)
        ShaderSurfaceCallback_(CurShaderClass_, MeshBuffer->getTextureLayerList());
    
    /* Get primitive count */
    D3DPRIMITIVETYPE PrimitiveType = D3DPT_TRIANGLELIST;
    u32 ArrayIndexCount = NumVertices;
    
    switch (MeshBuffer->getPrimitiveType())
    {
        case PRIMITIVE_TRIANGLES:
            PrimitiveType   = D3DPT_TRIANGLELIST;
            ArrayIndexCount = ArrayIndexCount / 3;
            break;
        case PRIMITIVE_TRIANGLE_STRIP:
            PrimitiveType   = D3DPT_TRIANGLESTRIP;
            ArrayIndexCount = ArrayIndexCount - 2;
            break;
        case PRIMITIVE_TRIANGLE_FAN:
            PrimitiveType   = D3DPT_TRIANGLEFAN;
            ArrayIndexCount = ArrayIndexCount - 2;
            break;
        case PRIMITIVE_LINES:
            PrimitiveType   = D3DPT_LINELIST;
            ArrayIndexCount = ArrayIndexCount / 2;
            break;
        case PRIMITIVE_LINE_STRIP:
            PrimitiveType   = D3DPT_LINESTRIP;
            ArrayIndexCount = ArrayIndexCount - 1;
            break;
        case PRIMITIVE_POINTS:
            PrimitiveType   = D3DPT_POINTLIST;
            break;
        default:
            return;
    }
    
    /* Draw the primitives */
    D3DDevice_->DrawPrimitive(PrimitiveType, StartOffset, ArrayIndexCount);
    
    #ifdef SP_COMPILE_WITH_RENDERSYS_QUERIES
    ++RenderSystem::NumDrawCalls_;
    #endif
}

void Direct3D9RenderSystem::drawMeshBuffer(const MeshBuffer* MeshBuffer)
{
    /* Get reference mesh buffer */
    if (!MeshBuffer)
        return;
    
    const video::MeshBuffer* OrigMeshBuffer = MeshBuffer;
    MeshBuffer = MeshBuffer->getReference();
    
    if (!MeshBuffer->renderable())
        return;
    
    /* Surface shader callback */
    if (CurShaderClass_ && ShaderSurfaceCallback_)
        ShaderSurfaceCallback_(CurShaderClass_, MeshBuffer->getTextureLayerList());
    
    /* Get hardware vertex- and index buffers */
    D3D9VertexBuffer* VertexBuffer = reinterpret_cast<D3D9VertexBuffer*>(MeshBuffer->getVertexBufferID());
    D3D9IndexBuffer* IndexBuffer   = reinterpret_cast<D3D9IndexBuffer*>(MeshBuffer->getIndexBufferID());
    
    /* Bind textures */
    if (__isTexturing)
        bindTextureLayers(OrigMeshBuffer->getTextureLayerList());
    else
        unbindPrevTextureLayers();
    
    /* Setup vertex format */
    D3DDevice_->SetFVF(VertexBuffer->FormatFlags_);
    
    /* Get primitive count */
    D3DPRIMITIVETYPE PrimitiveType  = D3DPT_TRIANGLELIST;
    u32 PrimitiveCount              = MeshBuffer->getIndexCount();
    u32 ArrayIndexCount             = MeshBuffer->getVertexCount();
    
    switch (MeshBuffer->getPrimitiveType())
    {
        case PRIMITIVE_TRIANGLES:
            PrimitiveType   = D3DPT_TRIANGLELIST;
            PrimitiveCount  = PrimitiveCount / 3;
            ArrayIndexCount = ArrayIndexCount / 3;
            break;
        case PRIMITIVE_TRIANGLE_STRIP:
            PrimitiveType   = D3DPT_TRIANGLESTRIP;
            PrimitiveCount  = PrimitiveCount - 2;
            ArrayIndexCount = ArrayIndexCount - 2;
            break;
        case PRIMITIVE_TRIANGLE_FAN:
            PrimitiveType   = D3DPT_TRIANGLEFAN;
            PrimitiveCount  = PrimitiveCount - 2;
            ArrayIndexCount = ArrayIndexCount - 2;
            break;
        case PRIMITIVE_LINES:
            PrimitiveType   = D3DPT_LINELIST;
            PrimitiveCount  = PrimitiveCount / 2;
            ArrayIndexCount = ArrayIndexCount / 2;
            break;
        case PRIMITIVE_LINE_STRIP:
            PrimitiveType   = D3DPT_LINESTRIP;
            PrimitiveCount  = PrimitiveCount - 1;
            ArrayIndexCount = ArrayIndexCount - 1;
            break;
        case PRIMITIVE_POINTS:
            PrimitiveType   = D3DPT_POINTLIST;
            break;
        default:
            return;
    }
    
    /* Check if hardware buffers are available */
    if (VertexBuffer->HWBuffer_)
    {
        /* Bind hardware mesh buffer */
        D3DDevice_->SetStreamSource(
            0, VertexBuffer->HWBuffer_,
            0, MeshBuffer->getVertexFormat()->getFormatSize()
        );
        
        /* Draw the primitives */
        if (MeshBuffer->getIndexBufferEnable() && IndexBuffer)
        {
            D3DDevice_->SetIndices(IndexBuffer->HWBuffer_);
            
            D3DDevice_->DrawIndexedPrimitive(
                PrimitiveType, 0, 0, MeshBuffer->getVertexCount(), 0, PrimitiveCount
            );
        }
        else
            D3DDevice_->DrawPrimitive(PrimitiveType, 0, ArrayIndexCount);
        
        /* Unbind hardware mesh buffer */
        D3DDevice_->SetStreamSource(0, 0, 0, 0);
        D3DDevice_->SetIndices(0);
    }
    else
    {
        /* Draw the primitives */
        if (MeshBuffer->getIndexBufferEnable())
        {
            D3DDevice_->DrawIndexedPrimitiveUP(
                D3DPT_TRIANGLELIST, 0,
                MeshBuffer->getVertexCount(),
                PrimitiveCount,
                MeshBuffer->getIndexBuffer().getArray(),
                IndexBuffer->FormatFlags_,
                MeshBuffer->getVertexBuffer().getArray(),
                MeshBuffer->getVertexFormat()->getFormatSize()
            );
        }
        else
        {
            D3DDevice_->DrawPrimitiveUP(
                D3DPT_TRIANGLELIST,
                ArrayIndexCount,
                MeshBuffer->getVertexBuffer().getArray(),
                MeshBuffer->getVertexFormat()->getFormatSize()
            );
        }
    }
    
    #ifdef SP_COMPILE_WITH_RENDERSYS_QUERIES
    ++RenderSystem::NumDrawCalls_;
    ++RenderSystem::NumMeshBufferBindings_;
    #endif
}


/* === Queries === */

Query* Direct3D9RenderSystem::createQuery(const EQueryTypes Type)
{
    /* Create new query object */
    Query* NewQuery = new Direct3D9Query(Type);
    QueryList_.push_back(NewQuery);

    /* Store in resource manager */
    ResMngr_.add(ResMngr_.Queries, NewQuery, static_cast<Direct3D9Query*>(NewQuery)->D3DQuery_);

    return NewQuery;
}

void Direct3D9RenderSystem::deleteQuery(Query* &QueryObj)
{
    if (QueryObj)
    {
        /* Remove from resource manager */
        ResMngr_.remove(ResMngr_.Queries, QueryObj);

        /* Delete query */
        RenderSystem::deleteQuery(QueryObj);
    }
}


/* === Render states === */

void Direct3D9RenderSystem::setRenderState(const video::ERenderStates Type, s32 State)
{
    switch (Type)
    {
        case RENDER_ALPHATEST:
            D3DDevice_->SetRenderState(D3DRS_ALPHATESTENABLE, State); break;
        case RENDER_BLEND:
            D3DDevice_->SetRenderState(D3DRS_ALPHABLENDENABLE, State); break;
        case RENDER_COLORMATERIAL:
            D3DDevice_->SetRenderState(D3DRS_COLORVERTEX, State); break;
        case RENDER_CULLFACE:
            D3DDevice_->SetRenderState(D3DRS_CULLMODE, State ? D3DCULL_CCW : D3DCULL_NONE); break;
        case RENDER_DEPTH:
            D3DDevice_->SetRenderState(D3DRS_ZENABLE, State); break;
        case RENDER_DITHER:
            D3DDevice_->SetRenderState(D3DRS_DITHERENABLE, State); break;
        case RENDER_FOG:
            D3DDevice_->SetRenderState(D3DRS_FOGENABLE, State); break;
        case RENDER_LIGHTING:
            D3DDevice_->SetRenderState(D3DRS_LIGHTING, State); break;
        case RENDER_LINESMOOTH:
            D3DDevice_->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, State); break;
        case RENDER_MULTISAMPLE:
            D3DDevice_->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, State); break;
        case RENDER_NORMALIZE:
        case RENDER_RESCALENORMAL:
            D3DDevice_->SetRenderState(D3DRS_NORMALIZENORMALS, State); break;
        case RENDER_POINTSMOOTH:
            break;
        case RENDER_SCISSOR:
            D3DDevice_->SetRenderState(D3DRS_SCISSORTESTENABLE, State); break;
        case RENDER_STENCIL:
            D3DDevice_->SetRenderState(D3DRS_STENCILENABLE, State); break;
        case RENDER_TEXTURE:
            __isTexturing = (State != 0); break;
    }
}

s32 Direct3D9RenderSystem::getRenderState(const video::ERenderStates Type) const
{
    DWORD State = 0;
    
    switch (Type)
    {
        case RENDER_ALPHATEST:
            D3DDevice_->GetRenderState(D3DRS_ALPHATESTENABLE, &State); break;
        case RENDER_BLEND:
            D3DDevice_->GetRenderState(D3DRS_ALPHABLENDENABLE, &State); break;
        case RENDER_COLORMATERIAL:
            D3DDevice_->GetRenderState(D3DRS_COLORVERTEX, &State); break;
        case RENDER_CULLFACE:
            D3DDevice_->GetRenderState(D3DRS_CULLMODE, &State);
            State = (State == D3DCULL_CCW ? 1 : 0);
            break;
        case RENDER_DEPTH:
            D3DDevice_->GetRenderState(D3DRS_ZENABLE, &State); break;
        case RENDER_DITHER:
            D3DDevice_->GetRenderState(D3DRS_DITHERENABLE, &State); break;
        case RENDER_FOG:
            D3DDevice_->GetRenderState(D3DRS_FOGENABLE, &State); break;
        case RENDER_LIGHTING:
            D3DDevice_->GetRenderState(D3DRS_LIGHTING, &State); break;
        case RENDER_LINESMOOTH:
            D3DDevice_->GetRenderState(D3DRS_ANTIALIASEDLINEENABLE, &State); break;
        case RENDER_MULTISAMPLE:
            D3DDevice_->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS, &State); break;
        case RENDER_NORMALIZE:
        case RENDER_RESCALENORMAL:
            D3DDevice_->GetRenderState(D3DRS_NORMALIZENORMALS, &State); break;
        case RENDER_POINTSMOOTH:
            break;
        case RENDER_SCISSOR:
            D3DDevice_->GetRenderState(D3DRS_SCISSORTESTENABLE, &State); break;
        case RENDER_STENCIL:
            D3DDevice_->GetRenderState(D3DRS_STENCILENABLE, &State); break;
        case RENDER_TEXTURE:
            return __isTexturing ? 1 : 0;
    }
    
    return State;
}

void Direct3D9RenderSystem::endSceneRendering()
{
    RenderSystem::endSceneRendering();
    
    /* Default render functions */
    D3DDevice_->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
    D3DDevice_->SetRenderState(D3DRS_ALPHAREF, 0);
    D3DDevice_->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
    D3DDevice_->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    
    PrevMaterial_ = 0;
}


/*
 * ======= Lighting =======
 */

void Direct3D9RenderSystem::addDynamicLightSource(
    u32 LightID, scene::ELightModels Type,
    video::color &Diffuse, video::color &Ambient, video::color &Specular,
    f32 AttenuationConstant, f32 AttenuationLinear, f32 AttenuationQuadratic)
{
    if (LightID >= MAX_COUNT_OF_LIGHTS)
        return;
    
    /* Set the lighting type */
    switch (Type)
    {
        case scene::LIGHT_DIRECTIONAL:
            D3DActiveLight_.Type = D3DLIGHT_DIRECTIONAL; break;
        case scene::LIGHT_POINT:
            D3DActiveLight_.Type = D3DLIGHT_POINT; break;
        case scene::LIGHT_SPOT:
            D3DActiveLight_.Type = D3DLIGHT_SPOT; break;
    }
    
    /* Default values */
    D3DActiveLight_.Range           = 1000.0f;
    D3DActiveLight_.Falloff         = 1.0f;
    D3DActiveLight_.Direction.z     = 1.0f;
    
    /* Lighting colors */
    D3DActiveLight_.Diffuse         = getD3DColor(Diffuse);
    D3DActiveLight_.Ambient         = getD3DColor(Ambient);
    D3DActiveLight_.Specular        = getD3DColor(Specular);
    
    /* Volumetric light attenuations */
    D3DActiveLight_.Attenuation0    = AttenuationConstant;
    D3DActiveLight_.Attenuation1    = AttenuationLinear;
    D3DActiveLight_.Attenuation2    = AttenuationQuadratic;
    
    /* Set the light attributes */
    D3DDevice_->SetLight(LightID, &D3DActiveLight_);
    
    /* Enable the light */
    D3DDevice_->LightEnable(LightID, true);
}

void Direct3D9RenderSystem::setLightStatus(u32 LightID, bool Enable, bool UseAllRCs)
{
    D3DDevice_->LightEnable(LightID, Enable);
}

void Direct3D9RenderSystem::setLightColor(
    u32 LightID, const video::color &Diffuse, const video::color &Ambient, const video::color &Specular, bool UseAllRCs)
{
    /* Get the light attributes */
    D3DDevice_->GetLight(LightID, &D3DActiveLight_);
    
    /* Lighting colors */
    D3DActiveLight_.Diffuse     = getD3DColor(Diffuse);
    D3DActiveLight_.Ambient     = getD3DColor(Ambient);
    D3DActiveLight_.Specular    = getD3DColor(Specular);
    
    /* Set the light attributes */
    D3DDevice_->SetLight(LightID, &D3DActiveLight_);
}


/*
 * ======= Fog effect =======
 */

void Direct3D9RenderSystem::setFog(const EFogTypes Type)
{
    /* Select the fog mode */
    switch (Fog_.Type = Type)
    {
        case FOG_NONE:
        {
            __isFog = false;
        }
        break;
        
        case FOG_STATIC:
        {
            __isFog = true;
            
            /* Set fog type */
            switch (Fog_.Mode)
            {
                case FOG_PALE:
                    D3DDevice_->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_EXP); break;
                case FOG_THICK:
                    D3DDevice_->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_EXP2); break;
            }
            
            /* Range settings */
            D3DDevice_->SetRenderState(D3DRS_FOGDENSITY, *(DWORD*)&Fog_.Range);
            D3DDevice_->SetRenderState(D3DRS_FOGSTART, *(DWORD*)&Fog_.Near);
            D3DDevice_->SetRenderState(D3DRS_FOGEND, *(DWORD*)&Fog_.Far);
        }
        break;
        
        case FOG_VOLUMETRIC:
        {
            __isFog = true;
            
            /* Renderer settings */
            D3DDevice_->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
            D3DDevice_->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR); // ???
            D3DDevice_->SetRenderState(D3DRS_FOGDENSITY, *(DWORD*)&Fog_.Range);
            D3DDevice_->SetRenderState(D3DRS_FOGSTART, 0);
            D3DDevice_->SetRenderState(D3DRS_FOGEND, 1);
        }
        break;
    }
}

void Direct3D9RenderSystem::setFogColor(const video::color &Color)
{
    D3DDevice_->SetRenderState(D3DRS_FOGCOLOR, Color.getSingle());
    Fog_.Color = Color;
}

void Direct3D9RenderSystem::setFogRange(f32 Range, f32 NearPlane, f32 FarPlane, const EFogModes Mode)
{
    RenderSystem::setFogRange(Range, NearPlane, FarPlane, Mode);
    
    if (Fog_.Type != FOG_VOLUMETRIC)
    {
        /* Set fog type */
        switch (Fog_.Mode)
        {
            case FOG_PALE:
                D3DDevice_->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_EXP); break;
            case FOG_THICK:
                D3DDevice_->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_EXP2); break;
        }
        
        /* Range settings */
        D3DDevice_->SetRenderState(D3DRS_FOGDENSITY, *(DWORD*)&Fog_.Range);
        D3DDevice_->SetRenderState(D3DRS_FOGSTART, *(DWORD*)&Fog_.Near);
        D3DDevice_->SetRenderState(D3DRS_FOGEND, *(DWORD*)&Fog_.Far);
    }
}


/* === Clipping planes === */

void Direct3D9RenderSystem::setClipPlane(u32 Index, const dim::plane3df &Plane, bool Enable)
{
    if (Index >= MaxClippingPlanes_)
        return;
    
    D3DDevice_->SetClipPlane(Index, (const f32*)&Plane);
    
    DWORD State = 0;
    DWORD Flag = (1 << Index);

    D3DDevice_->GetRenderState(D3DRS_CLIPPLANEENABLE, &State);
    
    if (Enable)
        math::addFlag(State, Flag);
    else
        math::removeFlag(State, Flag);
    
    D3DDevice_->SetRenderState(D3DRS_CLIPPLANEENABLE, State);
}



/*
 * ======= Shader programs =======
 */

ShaderClass* Direct3D9RenderSystem::createShaderClass(const VertexFormat* VertexInputLayout)
{
    ShaderClass* NewShaderClass = new Direct3D9ShaderClass();
    ShaderClassList_.push_back(NewShaderClass);
    return NewShaderClass;
}

Shader* Direct3D9RenderSystem::createShader(
    ShaderClass* ShaderClassObj, const EShaderTypes Type, const EShaderVersions Version,
    const std::list<io::stringc> &ShaderBuffer, const io::stringc &EntryPoint, u32 Flags)
{
    return createShaderObject<Direct3D9Shader>(
        ShaderClassObj, Type, Version, ShaderBuffer, EntryPoint, Flags
    );
}

Shader* Direct3D9RenderSystem::createCgShader(
    ShaderClass* ShaderClassObj, const EShaderTypes Type, const EShaderVersions Version,
    const std::list<io::stringc> &ShaderBuffer, const io::stringc &EntryPoint,
    const c8** CompilerOptions)
{
    Shader* NewShader = 0;
    
    #ifndef SP_COMPILE_WITH_CG
    io::Log::error("This engine was not compiled with the Cg toolkit");
    #else
    if (RenderQuery_[RENDERQUERY_SHADER])
        NewShader = new CgShaderProgramD3D9(ShaderClassObj, Type, Version);
    else
    #endif
        NewShader = new Shader(ShaderClassObj, Type, Version);
    
    NewShader->compile(ShaderBuffer, EntryPoint, CompilerOptions);
    
    if (!ShaderClassObj)
        NewShader->getShaderClass()->compile();
    
    ShaderList_.push_back(NewShader);
    
    return NewShader;
}

void Direct3D9RenderSystem::unbindShaders()
{
    D3DDevice_->SetVertexShader(0);
    D3DDevice_->SetPixelShader(0);
}


/*
 * ======= Drawing 2D objects =======
 */

void Direct3D9RenderSystem::beginDrawing2D()
{
    /* Setup alpha channel modulation for texture stages */
    D3DDevice_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    D3DDevice_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    
    D3DDevice_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    D3DDevice_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    D3DDevice_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    
    /* Unit matrices */
    setViewMatrix(dim::matrix4f::IDENTITY);
    setWorldMatrix(dim::matrix4f::IDENTITY);
    
    Matrix2D_.make2Dimensional(
        gSharedObjects.ScreenWidth,
        -gSharedObjects.ScreenHeight,
        gSharedObjects.ScreenWidth,
        gSharedObjects.ScreenHeight
    );
    setProjectionMatrix(Matrix2D_);
    
    /* Disable 3d render states */
    D3DDevice_->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    D3DDevice_->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    D3DDevice_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    
    /* Use no texture layer */
    D3DDevice_->SetTexture(0, 0);
    
    setViewport(0, dim::size2di(gSharedObjects.ScreenWidth, gSharedObjects.ScreenHeight));
    
    RenderSystem::beginDrawing2D();
}

void Direct3D9RenderSystem::setBlending(const EBlendingTypes SourceBlend, const EBlendingTypes DestBlend)
{
    D3DDevice_->SetRenderState(D3DRS_SRCBLEND, D3DBlendingList[SourceBlend]);
    D3DDevice_->SetRenderState(D3DRS_DESTBLEND, D3DBlendingList[DestBlend]);
}

void Direct3D9RenderSystem::setClipping(bool Enable, const dim::point2di &Position, const dim::size2di &Dimension)
{
    D3DDevice_->SetRenderState(D3DRS_SCISSORTESTENABLE, Enable);
    
    RECT rc;
    {
        rc.left     = Position.X;
        rc.top      = Position.Y;
        rc.right    = Position.X + Dimension.Width;
        rc.bottom   = Position.Y + Dimension.Height;
    }
    D3DDevice_->SetScissorRect(&rc);
}

void Direct3D9RenderSystem::setViewport(const dim::point2di &Position, const dim::size2di &Dimension)
{
    D3DVIEWPORT9 Viewport;
    {
        Viewport.X      = Position.X;
        Viewport.Y      = Position.Y;
        Viewport.Width  = Dimension.Width;
        Viewport.Height = Dimension.Height;
        Viewport.MinZ   = DepthRange_.Near;
        Viewport.MaxZ   = DepthRange_.Far;
    }
    D3DDevice_->SetViewport(&Viewport);
}

bool Direct3D9RenderSystem::setRenderTarget(Texture* Target)
{
    if (Target && Target->getRenderTarget())
    {
        if (!setRenderTargetSurface(0, Target))
            return false;
        
        const std::vector<Texture*>& MRTexList = Target->getMultiRenderTargets();
        
        for (u32 i = 0; i < MRTexList.size(); ++i)
        {
            if (!setRenderTargetSurface(i + 1, MRTexList[i]))
                return false;
        }
        
        RenderTarget_ = Target;
    }
    else if (RenderTarget_ && PrevRenderTargetSurface_)
    {
        /* Set the last render target */
        D3DDevice_->SetRenderTarget(0, PrevRenderTargetSurface_);
        Direct3D9RenderSystem::releaseObject(PrevRenderTargetSurface_);
        
        const u32 RTCount = RenderTarget_->getMultiRenderTargets().size() + 1;
        
        for (u32 i = 1; i < RTCount && i < DevCaps_.NumSimultaneousRTs; ++i)
            D3DDevice_->SetRenderTarget(i, 0);
        
        RenderTarget_ = 0;
    }
    
    return true;
}

void Direct3D9RenderSystem::setPointSize(s32 Size)
{
    f32 Temp = static_cast<f32>(Size);
    D3DDevice_->SetRenderState(D3DRS_POINTSIZE, *(DWORD*)(&Temp));
}


/*
 * ======= Image drawing =======
 */

void Direct3D9RenderSystem::draw2DImage(
    const Texture* Tex, const dim::point2di &Position, const color &Color)
{
    if (Tex)
    {
        const s32 Width     = Tex->getSize().Width;
        const s32 Height    = Tex->getSize().Height;
        
        draw2DImage(
            Tex, dim::rect2di(Position.X, Position.Y, Width, Height), dim::rect2df(0, 0, 1, 1), Color
        );
    }
}

void Direct3D9RenderSystem::draw2DImage(
    const Texture* Tex, const dim::rect2di &Position, const dim::rect2df &Clipping, const color &Color)
{
    /* Setup 2d drawing */
    if (!Tex)
        return;
    
    setup2DDrawing();
    
    /* Bind the texture */
    Tex->bind();
    
    /* Set the vertex data */
    const dim::rect2df RectF(Position.cast<f32>());
    const u32 Clr = Color.getSingle();
    
    SPrimitiveVertex VerticesList[4] =
    {
        SPrimitiveVertex(RectF.Left,               RectF.Top,                0.0f, Clr, Clipping.Left,  Clipping.Top    ),
        SPrimitiveVertex(RectF.Left + RectF.Right, RectF.Top,                0.0f, Clr, Clipping.Right, Clipping.Top    ),
        SPrimitiveVertex(RectF.Left + RectF.Right, RectF.Top + RectF.Bottom, 0.0f, Clr, Clipping.Right, Clipping.Bottom ),
        SPrimitiveVertex(RectF.Left,               RectF.Top + RectF.Bottom, 0.0f, Clr, Clipping.Left,  Clipping.Bottom )
    };
    
    /* Set the render states */
    D3DDevice_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    
    /* Update the primitive list */
    updatePrimitiveList(VerticesList, 4);
    
    /* Draw the rectangle */
    D3DDevice_->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    
    /* Unbind the texture */
    Tex->unbind();
}

void Direct3D9RenderSystem::draw2DImage(
    const Texture* Tex, const dim::point2di &Position, f32 Rotation, f32 Radius, const color &Color)
{
    /* Setup 2d drawing */
    if (!Tex)
        return;
    
    setup2DDrawing();
    
    /* Bind the texture */
    Tex->bind();
    
    /* Temporary variables */
    u32 Clr = Color.getSingle();
    
    /* Set the vertex data */
    dim::matrix2f Matrix;
    Matrix.rotate(Rotation);
    Matrix.scale(Radius);
    
    const dim::point2df PosF(Position.cast<f32>());
    
    const dim::point2df PointLeftTop    (PosF + Matrix * dim::point2df(-1.0f, -1.0f));
    const dim::point2df PointRightTop   (PosF + Matrix * dim::point2df( 1.0f, -1.0f));
    const dim::point2df PointRightBottom(PosF + Matrix * dim::point2df( 1.0f,  1.0f));
    const dim::point2df PointLeftBottom (PosF + Matrix * dim::point2df(-1.0f,  1.0f));
    
    SPrimitiveVertex VerticesList[4] =
    {
        SPrimitiveVertex(PointLeftTop.X,        PointLeftTop.Y,     0.0f, Clr, 0.0f, 0.0f ),
        SPrimitiveVertex(PointRightTop.X,       PointRightTop.Y,    0.0f, Clr, 1.0f, 0.0f ),
        SPrimitiveVertex(PointRightBottom.X,    PointRightBottom.Y, 0.0f, Clr, 1.0f, 1.0f ),
        SPrimitiveVertex(PointLeftBottom.X,     PointLeftBottom.Y,  0.0f, Clr, 0.0f, 1.0f )
    };
    
    /* Set the render states */
    D3DDevice_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    
    /* Update the primitive list */
    updatePrimitiveList(VerticesList, 4);
    
    /* Draw the rectangle */
    D3DDevice_->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    
    /* Unbind the texture */
    Tex->unbind();
}

void Direct3D9RenderSystem::draw2DImage(
    const Texture* Tex,
    const dim::point2di &lefttopPosition, const dim::point2di &righttopPosition,
    const dim::point2di &rightbottomPosition, const dim::point2di &leftbottomPosition,
    const dim::point2df &lefttopClipping, const dim::point2df &righttopClipping,
    const dim::point2df &rightbottomClipping, const dim::point2df &leftbottomClipping,
    const color &lefttopColor, const color &righttopColor,
    const color &rightbottomColor, const color &leftbottomColor)
{
    /* Setup 2d drawing */
    if (!Tex)
        return;
    
    setup2DDrawing();
    
    /* Bind the texture */
    Tex->bind();
    
    /* Set the vertex data */
    SPrimitiveVertex VerticesList[4] = {
        SPrimitiveVertex( (f32)lefttopPosition.X, (f32)lefttopPosition.Y, 0.0f, lefttopColor.getSingle(), lefttopClipping.X, lefttopClipping.Y ),
        SPrimitiveVertex( (f32)righttopPosition.X, (f32)righttopPosition.Y, 0.0f, righttopColor.getSingle(), righttopClipping.X, righttopClipping.Y ),
        SPrimitiveVertex( (f32)rightbottomPosition.X, (f32)rightbottomPosition.Y, 0.0f, rightbottomColor.getSingle(), rightbottomClipping.X, rightbottomClipping.Y ),
        SPrimitiveVertex( (f32)leftbottomPosition.X, (f32)leftbottomPosition.Y, 0.0f, leftbottomColor.getSingle(), leftbottomClipping.X, leftbottomClipping.Y )
    };
    
    /* Set the render states */
    D3DDevice_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    
    /* Update the primitive list */
    updatePrimitiveList(VerticesList, 4);
    
    /* Draw the rectangle */
    D3DDevice_->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    
    /* Unbind the texture */
    Tex->unbind();
}


/*
 * ======= Primitive drawing =======
 */

color Direct3D9RenderSystem::getPixelColor(const dim::point2di &Position) const
{
    return color(0); // todo
}
f32 Direct3D9RenderSystem::getPixelDepth(const dim::point2di &Position) const
{
    return 0.0f; // todo
}

void Direct3D9RenderSystem::draw2DPoint(const dim::point2di &Position, const color &Color)
{
    setup2DDrawing();
    
    /* Set the vertex data */
    const SPrimitiveVertex VerticesList(
        static_cast<f32>(Position.X), static_cast<f32>(Position.Y), 0.0f, Color.getSingle()
    );
    
    /* Update the primitive list */
    updatePrimitiveList(&VerticesList, 1);
    
    /* Draw the rectangle */
    D3DDevice_->DrawPrimitive(D3DPT_POINTLIST, 0, 1);
}

void Direct3D9RenderSystem::draw2DLine(
    const dim::point2di &PositionA, const dim::point2di &PositionB, const color &Color)
{
    draw2DLine(PositionA, PositionB, Color, Color);
}

void Direct3D9RenderSystem::draw2DLine(
    const dim::point2di &PositionA, const dim::point2di &PositionB, const color &ColorA, const color &ColorB)
{
    setup2DDrawing();
    
    /* Set the vertex data */
    SPrimitiveVertex VerticesList[2] = {
        SPrimitiveVertex( (f32)PositionA.X, (f32)PositionA.Y, 0.0f, ColorA.getSingle() ),
        SPrimitiveVertex( (f32)PositionB.X, (f32)PositionB.Y, 0.0f, ColorB.getSingle() ),
    };
    
    /* Update the primitive list */
    updatePrimitiveList(VerticesList, 2);
    
    /* Draw the rectangle */
    D3DDevice_->DrawPrimitive(D3DPT_LINELIST, 0, 1);
}

void Direct3D9RenderSystem::draw2DLine(
    const dim::point2di &PositionA, const dim::point2di &PositionB, const color &Color, s32 DotLength)
{
    draw2DLine(PositionA, PositionB, Color, Color); // !!!
}

void Direct3D9RenderSystem::draw2DRectangle(
    const dim::rect2di &Rect, const color &Color, bool isSolid)
{
    draw2DRectangle(Rect, Color, Color, Color, Color, isSolid);
}

void Direct3D9RenderSystem::draw2DRectangle(
    const dim::rect2di &Rect, const color &lefttopColor, const color &righttopColor,
    const color &rightbottomColor, const color &leftbottomColor, bool isSolid)
{
    setup2DDrawing();
    
    /* Set the vertex data */
    const dim::rect2df RectF(Rect.cast<f32>());
    
    const SPrimitiveVertex VerticesList[4] =
    {
        SPrimitiveVertex(RectF.Left, RectF.Top, 0.0f, lefttopColor.getSingle() ),
        SPrimitiveVertex(RectF.Right, RectF.Top, 0.0f, righttopColor.getSingle() ),
        SPrimitiveVertex(RectF.Right, RectF.Bottom, 0.0f, rightbottomColor.getSingle() ),
        SPrimitiveVertex(RectF.Left, RectF.Bottom, 0.0f, leftbottomColor.getSingle() )
    };
    
    /* Set the render states */
    D3DDevice_->SetRenderState(D3DRS_FILLMODE, isSolid ? D3DFILL_SOLID : D3DFILL_WIREFRAME);
    
    /* Update the primitive list */
    updatePrimitiveList(VerticesList, 4);
    
    /* Draw the rectangle */
    D3DDevice_->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
}


/*
 * ======= Extra drawing functions =======
 */

void Direct3D9RenderSystem::draw2DPolygon(
    const ERenderPrimitives Type, const scene::SPrimitiveVertex2D* VerticesList, u32 Count)
{
    if (!VerticesList || !Count)
        return;
    
    setup2DDrawing();
    
    /* Select the primitive type */
    D3DPRIMITIVETYPE Mode;
    UINT PrimitiveCount = 0;
    
    switch (Type)
    {
        case PRIMITIVE_POINTS:
            Mode = D3DPT_POINTLIST, PrimitiveCount = Count; break;
        case PRIMITIVE_LINES:
            Mode = D3DPT_LINELIST, PrimitiveCount = Count/2; break;
        case PRIMITIVE_LINE_STRIP:
            Mode = D3DPT_LINESTRIP, PrimitiveCount = Count/2 + 1; break;
        case PRIMITIVE_TRIANGLES:
            Mode = D3DPT_TRIANGLELIST, PrimitiveCount = Count/3; break;
        case PRIMITIVE_TRIANGLE_STRIP:
            Mode = D3DPT_TRIANGLESTRIP, PrimitiveCount = Count - 2; break;
        case PRIMITIVE_TRIANGLE_FAN:
            Mode = D3DPT_TRIANGLEFAN, PrimitiveCount = Count - 2; break;
        case PRIMITIVE_LINE_LOOP:
        case PRIMITIVE_QUADS:
        case PRIMITIVE_QUAD_STRIP:
        case PRIMITIVE_POLYGON:
        default:
            return;
    }
    
    /* Setup the FVF for 2D graphics */
    D3DDevice_->SetFVF(FVF_VERTEX2D);
    
    /* Render primitives */
    D3DDevice_->DrawPrimitiveUP(
        Mode,
        PrimitiveCount,
        VerticesList,
        sizeof(scene::SPrimitiveVertex2D)
    );
}

void Direct3D9RenderSystem::draw2DPolygonImage(
    const ERenderPrimitives Type, Texture* Tex, const scene::SPrimitiveVertex2D* VerticesList, u32 Count)
{
    if (Tex)
    {
        Tex->bind();
        draw2DPolygon(Type, VerticesList, Count);
        Tex->unbind();
    }
    else
        draw2DPolygon(Type, VerticesList, Count);
}


/*
 * ======= 3D drawing functions =======
 */

void Direct3D9RenderSystem::draw3DPoint(const dim::vector3df &Position, const color &Color)
{
    setup3DDrawing();
    
    /* Set the FVF (Flexible Vertex Format) */
    D3DDevice_->SetFVF(FVF_VERTEX3D);
    
    /* Set the vertices data */
    scene::SMeshVertex3D VerticesList[1] = {
        scene::SMeshVertex3D( Position.X, Position.Y, Position.Z, Color.getSingle() )
    };
    
    /* Draw the primitive */
    D3DDevice_->DrawPrimitiveUP(D3DPT_POINTLIST, 1, VerticesList, sizeof(scene::SMeshVertex3D));
}

void Direct3D9RenderSystem::draw3DLine(
    const dim::vector3df &PositionA, const dim::vector3df &PositionB, const color &Color)
{
    setup3DDrawing();
    
    /* Set the FVF (Flexible Vertex Format) */
    D3DDevice_->SetFVF(FVF_VERTEX3D);
    
    /* Set the vertices data */
    scene::SMeshVertex3D VerticesList[2] = {
        scene::SMeshVertex3D( PositionA.X, PositionA.Y, PositionA.Z, Color.getSingle() ),
        scene::SMeshVertex3D( PositionB.X, PositionB.Y, PositionB.Z, Color.getSingle() )
    };
    
    /* Draw the primitive */
    D3DDevice_->DrawPrimitiveUP(D3DPT_LINELIST, 1, VerticesList, sizeof(scene::SMeshVertex3D));
}

void Direct3D9RenderSystem::draw3DLine(
    const dim::vector3df &PositionA, const dim::vector3df &PositionB, const color &ColorA, const color &ColorB)
{
    setup3DDrawing();
    
    /* Set the FVF (Flexible Vertex Format) */
    D3DDevice_->SetFVF(FVF_VERTEX3D);
    
    /* Set the vertices data */
    scene::SMeshVertex3D VerticesList[2] = {
        scene::SMeshVertex3D( PositionA.X, PositionA.Y, PositionA.Z, ColorA.getSingle() ),
        scene::SMeshVertex3D( PositionB.X, PositionB.Y, PositionB.Z, ColorB.getSingle() ),
    };
    
    /* Draw the primitive */
    D3DDevice_->DrawPrimitiveUP(D3DPT_LINELIST, 1, VerticesList, sizeof(scene::SMeshVertex3D));
}

void Direct3D9RenderSystem::draw3DEllipse(
    const dim::vector3df &Position, const dim::vector3df &Rotation, const dim::size2df &Radius, const color &Color)
{
    // todo
}

void Direct3D9RenderSystem::draw3DTriangle(
    Texture* hTexture, const dim::triangle3df &Triangle, const color &Color)
{
    // todo
}


/*
 * ======= Texture loading & creating =======
 */

Texture* Direct3D9RenderSystem::createTexture(const STextureCreationFlags &CreationFlags)
{
    #if 0

    Texture* NewTexture = 0;
    
    /* Direct3D9 texture configurations */
    const dim::vector3di Size(CreationFlags.getSizeVec());
    
    if (createRendererTexture(CreationFlags.Filter.HasMIPMaps, TEXTURE_2D, Size, CreationFlags.Format, 0))
    {
        NewTexture = new Direct3D9Texture(
            CurD3DTexture_, CurD3DCubeTexture_, CurD3DVolumeTexture_, CreationFlags
        );
        
        if (CreationFlags.Filter.Anisotropy > 0)
            NewTexture->setAnisotropicSamples(CreationFlags.Filter.Anisotropy);
    }
    else
        NewTexture = new Direct3D9Texture();
    
    /* Store texture in resource manager */
    if (NewTexture)
    {
        IDirect3DBaseTexture9* D3DRes = static_cast<Direct3D9Texture*>(NewTexture)->D3DResource_.Res;

        if (D3DRes)
            ResMngr_.add(ResMngr_.TextureResources, NewTexture, D3DRes);
    }

    /* Add the texture to the texture list */
    TextureList_.push_back(NewTexture);
    
    return NewTexture;

    #else

    /* Create Direct3D9 texture */
    Texture* NewTexture = new Direct3D9Texture(CreationFlags);
    
    /* Add the texture to the texture list */
    TextureListSemaphore_.lock();
    TextureList_.push_back(NewTexture);
    TextureListSemaphore_.unlock();
    
    return NewTexture;

    #endif
}

Texture* Direct3D9RenderSystem::createScreenShot(const dim::point2di &Position, dim::size2di Size)
{
    Texture* NewTexture = RenderSystem::createTexture(Size);
    
    createScreenShot(NewTexture, Position);
    
    #ifdef SP_DEBUGMODE
    io::Log::debug("Direct3D9RenderSystem::createScreenShot", "Incomplete");
    #endif
    //!TODO!
    
    /* Return the texture & exit the function */
    return NewTexture;
}

void Direct3D9RenderSystem::createScreenShot(Texture* Tex, const dim::point2di &Position)
{
    #if 0

    /* Get the Direct3D texture handle */
    Direct3D9Texture* D3DTex = static_cast<Direct3D9Texture*>(Tex);

    if (!D3DTex->is2D())
        return;

    IDirect3DTexture9* DxTex = D3DTex->D3DResource_.Tex2D;
    
    if (!DxTex)
        return;
    
    IDirect3DSurface9* Surface = 0;
    
    D3DDISPLAYMODE DisplayMode;
    D3DDevice_->GetDisplayMode(0, &DisplayMode);
    
    if (D3DDevice_->CreateOffscreenPlainSurface(
        DisplayMode.Width, DisplayMode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &Surface, 0))
    {
        io::Log::error("Could not create Direct3D9 offscreen plain surface");
        return;
    }
    
    if (D3DDevice_->GetFrontBufferData(0, Surface))
    {
        Surface->Release();
        io::Log::error("Could not get front buffer data from Direct3D9 surface");
        return;
    }
    
    Surface->Release();

    #elif defined(SP_DEBUGMODE)

    io::Log::debug("Direct3D9RenderSystem::createScreenShot", "Incomplete");

    #endif
}

void Direct3D9RenderSystem::deleteTexture(Texture* &Tex)
{
    if (Tex)
    {
        /* Remove texture from resource manager */
        ResMngr_.remove(ResMngr_.TextureResources, Tex);

        /* Delete texture */
        RenderSystem::deleteTexture(Tex);
    }
}


/*
 * ======= Font loading and text drawing =======
 */

Font* Direct3D9RenderSystem::createBitmapFont(const io::stringc &FontName, s32 FontSize, s32 Flags)
{
    /* Temporary variables */
    HRESULT Result = S_OK;
    
    if (FontSize <= 0)
        FontSize = DEF_FONT_SIZE;
    
    const s32 Width     = 0;
    const s32 Height    = FontSize;
    
    /* Create the Direct3D font */
    
    ID3DXFont* DxFont = 0;
    
    #if D3DX_SDK_VERSION < 24
    
    #ifdef _MSC_VER
    #pragma comment (lib, "d3dx9.lib")
    #endif
    
    Result = D3DXCreateFont(
        D3DDevice_, Height, Width,
        isBold ? FW_BOLD : 0, 0, isItalic,
        isSymbolUsing ? SYMBOL_CHARSET : ANSI_CHARSET,
        OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH,
        FontName.c_str(), &DxFont
    );
    
    #else
    
    typedef HRESULT (WINAPI *PFND3DXCREATEFONTW)(
        IDirect3DDevice9* pDevice, INT Height, UINT Width, UINT Weight, UINT MipLevels, BOOL Italic,
        DWORD CharSet, DWORD OutputPrecision, DWORD Quality, DWORD PitchAndFamily, LPCWSTR pFacename, LPD3DXFONT * ppFont
    );
    typedef HRESULT (WINAPI *PFND3DXCREATEFONTA)(
        IDirect3DDevice9* pDevice, INT Height, UINT Width, UINT Weight, UINT MipLevels, BOOL Italic,
        DWORD CharSet, DWORD OutputPrecision, DWORD Quality, DWORD PitchAndFamily, LPCTSTR pFacename, LPD3DXFONT * ppFont
    );
    
    static PFND3DXCREATEFONTW pFncCreateFontW = 0;
    static PFND3DXCREATEFONTA pFncCreateFontA = 0;
    
    if (!pFncCreateFontW)
    {
        HMODULE hModule = LoadLibrary(d3dDllFileName.c_str());
        
        if (hModule)
        {
            pFncCreateFontW = (PFND3DXCREATEFONTW)GetProcAddress(hModule, "D3DXCreateFontW");
            
            if (!pFncCreateFontW)
            {
                io::Log::warning(
                    "Could not load function \"D3DXCreateFontW\" from Direct3D9 library file: \"" +
                    d3dDllFileName + "\", unicode is not supported"
                );
                
                pFncCreateFontA = (PFND3DXCREATEFONTA)GetProcAddress(hModule, "D3DXCreateFontA");
                
                if (!pFncCreateFontA)
                {
                    io::Log::error(
                        "Could not load function \"D3DXCreateFontA\" from Direct3D9 library file: \"" +
                        d3dDllFileName + "\""
                    );
                }
            }
        }
    }
    
    if (pFncCreateFontW)
    {
        Result = pFncCreateFontW(
            D3DDevice_,
            Height,
            Width,
            (Flags & FONT_BOLD) != 0 ? FW_BOLD : FW_NORMAL,
            0,
            (Flags & FONT_ITALIC) != 0,
            (Flags & FONT_SYMBOLS) != 0 ? SYMBOL_CHARSET : ANSI_CHARSET,
            OUT_TT_ONLY_PRECIS,
            ANTIALIASED_QUALITY,
            FF_DONTCARE | DEFAULT_PITCH,
            FontName.toUnicode().c_str(),
            &DxFont
        );
    }
    else if (pFncCreateFontA)
    {
        Result = pFncCreateFontA(
            D3DDevice_,
            Height,
            Width,
            (Flags & FONT_BOLD) != 0 ? FW_BOLD : FW_NORMAL,
            0,
            (Flags & FONT_ITALIC) != 0,
            (Flags & FONT_SYMBOLS) != 0 ? SYMBOL_CHARSET : ANSI_CHARSET,
            OUT_TT_ONLY_PRECIS,
            ANTIALIASED_QUALITY,
            FF_DONTCARE | DEFAULT_PITCH,
            FontName.c_str(),
            &DxFont
        );
    }
    
    #endif
    
    /* Check for errors */
    if (Result != S_OK)
        io::Log::error("Could not load font: \"" + FontName + "\"");
    
    /* Create device font */
    HFONT FontObject = 0;
    createDeviceFont(&FontObject, FontName, dim::size2di(Width, Height), Flags);
    
    //if (DxFont)
    //    DeviceContext_ = DxFont->GetDC();
    
    /* Create new font */
    Font* NewFont = new Font(DxFont, FontName, dim::size2di(Width, Height), getCharWidths(&FontObject));
    FontList_.push_back(NewFont);
    
    /* Delete device font object */
    DeleteObject(FontObject);
    
    return NewFont;
}


/*
 * ======= Matrix controll =======
 */

void Direct3D9RenderSystem::updateModelviewMatrix()
{
    D3DDevice_->SetTransform(D3DTS_VIEW, D3D_MATRIX(scene::spViewMatrix));
    D3DDevice_->SetTransform(D3DTS_WORLD, D3D_MATRIX(scene::spWorldMatrix));
}

void Direct3D9RenderSystem::setProjectionMatrix(const dim::matrix4f &Matrix)
{
    scene::spProjectionMatrix = Matrix;
    D3DDevice_->SetTransform(D3DTS_PROJECTION, D3D_MATRIX(Matrix));
}
void Direct3D9RenderSystem::setViewMatrix(const dim::matrix4f &Matrix)
{
    RenderSystem::setViewMatrix(Matrix);
    D3DDevice_->SetTransform(D3DTS_VIEW, D3D_MATRIX(Matrix));
}
void Direct3D9RenderSystem::setWorldMatrix(const dim::matrix4f &Matrix)
{
    scene::spWorldMatrix = Matrix;
    D3DDevice_->SetTransform(D3DTS_WORLD, D3D_MATRIX(Matrix));
}
void Direct3D9RenderSystem::setTextureMatrix(const dim::matrix4f &Matrix, u8 TextureLayer)
{
    scene::spTextureMatrix[TextureLayer] = Matrix;
    D3DDevice_->SetTransform(
        (D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + TextureLayer), D3D_MATRIX(Matrix)//.getTextureMatrix())
    );
}

void Direct3D9RenderSystem::releaseAllResources()
{
    /* Release default resources */
    Direct3D9RenderSystem::releaseObject(D3DDefFlexibleVertexBuffer_);
    Direct3D9RenderSystem::releaseObject(D3DDefVertexBuffer_);
    
    /* Release managed resources */
    ResMngr_.releaseAll();
}

void Direct3D9RenderSystem::recreateAllResources()
{
    /* ReCreate all queries */
    for (std::map<Query*, IDirect3DQuery9*>::iterator it = ResMngr_.Queries.begin(); it != ResMngr_.Queries.end(); ++it)
        static_cast<Direct3D9Query*>(it->first)->createHWQuery();
    
    //todo...

    #ifdef SP_DEBUGMODE
    io::Log::debug("Direct3D9RenderSystem::recreateAllResources", "Incomplete");
    #endif
}


/*
 * ======= Private functions =======
 */

void Direct3D9RenderSystem::updatePrimitiveList(const SPrimitiveVertex* VertexList, u32 Size)
{
    /* Fill the standard vertex buffer */
    VOID* pVoid;
    D3DDefVertexBuffer_->Lock(0, sizeof(SPrimitiveVertex)*Size, (void**)&pVoid, 0);
    memcpy(pVoid, VertexList, sizeof(SPrimitiveVertex)*Size);
    D3DDefVertexBuffer_->Unlock();
    
    /* Setup the FVF for 2D graphics */
    D3DDevice_->SetFVF(FVF_VERTEX2D);
    
    /* Set the stream souce */
    D3DDevice_->SetStreamSource(0, D3DDefVertexBuffer_, 0, sizeof(SPrimitiveVertex));
}

void Direct3D9RenderSystem::updatePrimitiveListFlexible(const SPrimitiveVertex* VertexList, u32 Count)
{
    /* Delete the old vertex buffer */
    Direct3D9RenderSystem::releaseObject(D3DDefFlexibleVertexBuffer_);
    
    /* Create a new vertex buffer */
    D3DDevice_->CreateVertexBuffer(
        sizeof(SPrimitiveVertex)*Count,
        0,
        FVF_VERTEX2D,
        D3DPOOL_DEFAULT,
        &D3DDefFlexibleVertexBuffer_,
        0
    );
    
    if (!D3DDefFlexibleVertexBuffer_)
    {
        io::Log::error("Could not create Direct3D9 vertex buffer");
        return;
    }
    
    /* Fill the standard vertex buffer */
    VOID* pVoid;
    D3DDefFlexibleVertexBuffer_->Lock(0, sizeof(SPrimitiveVertex)*Count, (void**)&pVoid, 0);
    memcpy(pVoid, VertexList, sizeof(SPrimitiveVertex)*Count);
    D3DDefFlexibleVertexBuffer_->Unlock();
    
    /* Setup the FVF for 2D graphics */
    D3DDevice_->SetFVF(FVF_VERTEX2D);
    
    /* Set the stream souce */
    D3DDevice_->SetStreamSource(0, D3DDefFlexibleVertexBuffer_, 0, sizeof(SPrimitiveVertex));
}

bool Direct3D9RenderSystem::setRenderTargetSurface(const s32 Index, Texture* Target)
{
    if (!PrevRenderTargetSurface_ && !Index)
        D3DDevice_->GetRenderTarget(0, &PrevRenderTargetSurface_);
    
    IDirect3DSurface9* Surface = 0;
    HRESULT Error = 0;
    
    if (Target->getType() == TEXTURE_CUBEMAP)
    {
        IDirect3DCubeTexture9* d3dTexture = static_cast<Direct3D9Texture*>(Target)->D3DResource_.TexCube;
        Error = d3dTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)Target->getCubeMapFace(), 0, &Surface);
    }
    else if (Target->getType() == TEXTURE_3D)
    {
        io::Log::error("Volume texture render targets are not supported for Direct3D9 yet");
        return false;
    }
    else
    {
        IDirect3DTexture9* d3dTexture = static_cast<Direct3D9Texture*>(Target)->D3DResource_.Tex2D;
        Error = d3dTexture->GetSurfaceLevel(0, &Surface);
    }
    
    if (Error)
    {
        io::Log::error("Could not get first surface level");
        return false;
    }
    
    /* Set the render target */
    if (D3DDevice_->SetRenderTarget(Index, Surface) == D3DERR_INVALIDCALL)
    {
        io::Log::error("Could not set render target");
        return false;
    }
    
    return true;
}

void Direct3D9RenderSystem::releaseFontObject(Font* FontObj)
{
    if (FontObj && FontObj->getBufferRawData())
    {
        if (FontObj->getTexture())
        {
            //todo
        }
        else
        {
            /* Release the Direct3D9 font */
            ID3DXFont* DxFont = reinterpret_cast<ID3DXFont*>(FontObj->getBufferRawData());
            Direct3D9RenderSystem::releaseObject(DxFont);
        }
    }
}

void Direct3D9RenderSystem::drawTexturedFont(
    const Font* FontObj, const dim::point2di &Position, const io::stringc &Text, const color &Color)
{
    setup2DDrawing();
    
    /* Get vertex buffer and glyph list */
    D3D9VertexBuffer* VertexBuffer = reinterpret_cast<D3D9VertexBuffer*>(FontObj->getBufferRawData());
    
    const SFontGlyph* GlyphList = &(FontObj->getGlyphList()[0]);
    
    /* Setup render- and texture states */
    D3DDevice_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    
    bindDrawingColor(Color);
    
    /* Setup vertex buffer source */
    D3DDevice_->SetFVF(FVF_VERTEX_FONT);
    D3DDevice_->SetStreamSource(0, VertexBuffer->HWBuffer_, 0, sizeof(dim::vector3df) + sizeof(dim::point2df));
    
    /* Bind texture */
    FontObj->getTexture()->bind(0);
    
    /* Initialize transformation */
    dim::matrix4f Transform;
    Transform.translate(dim::vector3df(static_cast<f32>(Position.X), static_cast<f32>(Position.Y), 0.0f));
    Transform *= FontTransform_;
    
    /* Draw each character */
    f32 Move = 0.0f;
    
    for (u32 i = 0, c = Text.size(); i < c; ++i)
    {
        /* Get character glyph from string */
        const u32 CurChar = static_cast<u32>(static_cast<u8>(Text[i]));
        const SFontGlyph* Glyph = &(GlyphList[CurChar]);
        
        /* Offset movement */
        Move += static_cast<f32>(Glyph->StartOffset);
        Transform.translate(dim::vector3df(Move, 0.0f, 0.0f));
        Move = 0.0f;
        
        /* Draw current character with current transformation */
        D3DDevice_->SetTransform(D3DTS_WORLD, D3D_MATRIX(Transform));
        D3DDevice_->DrawPrimitive(D3DPT_TRIANGLESTRIP, CurChar*4, 2);
        
        /* Character width and white space movement */
        Move += static_cast<f32>(Glyph->DrawnWidth + Glyph->WhiteSpace);
    }
    
    /* Reset world matrix */
    D3DDevice_->SetTransform(D3DTS_WORLD, D3D_MATRIX(scene::spWorldMatrix));
    
    /* Unbind vertex buffer */
    D3DDevice_->SetStreamSource(0, 0, 0, 0);
    
    /* Unbind texture */
    FontObj->getTexture()->unbind(0);
    
    unbindDrawingColor();
}

void Direct3D9RenderSystem::drawBitmapFont(
    const Font* FontObj, const dim::point2di &Position, const io::stringc &Text, const color &Color)
{
    ID3DXFont* DxFont = reinterpret_cast<ID3DXFont*>(FontObj->getBufferRawData());
    
    if (!DxFont)
        return;
    
    /* Setup drawing area */
    RECT rc;
    rc.left     = Position.X;
    rc.top      = Position.Y;
    rc.right    = gSharedObjects.ScreenWidth;
    rc.bottom   = gSharedObjects.ScreenHeight;
    
    /* Draw bitmap text */
    DxFont->DrawText(
        0, Text.c_str(), Text.size(), &rc, DT_LEFT | DT_TOP | DT_SINGLELINE, Color.getSingle()
    );
}

// Direct3D 9 font glyph vertex format
struct SFontGlyphVertexD3D9
{
    dim::vector3df Position;
    dim::point2df TexCoord;
};

void Direct3D9RenderSystem::createTexturedFontVertexBuffer(dim::UniversalBuffer &VertexBuffer, VertexFormatUniversal &VertFormat)
{
    /* D3D9 vertex buffer for textured font glyphs */
    VertexBuffer.setStride(sizeof(SFontGlyphVertexD3D9));
    
    VertFormat.addCoord(DATATYPE_FLOAT, 3);
    VertFormat.addTexCoord();
}

void Direct3D9RenderSystem::setupTexturedFontGlyph(
    void* &RawVertexData, const SFontGlyph &Glyph, const dim::rect2df &Mapping)
{
    SFontGlyphVertexD3D9* VertexData = reinterpret_cast<SFontGlyphVertexD3D9*>(RawVertexData);
    
    VertexData[0].Position = dim::vector3df(0.0f);
    VertexData[1].Position = dim::vector3df(
        static_cast<f32>(Glyph.Rect.Right - Glyph.Rect.Left), 0.0f, 0.0f
    );
    VertexData[2].Position = dim::vector3df(
        0.0f, static_cast<f32>(Glyph.Rect.Bottom - Glyph.Rect.Top), 0.0f
    );
    VertexData[3].Position = dim::vector3df(
        static_cast<f32>(Glyph.Rect.Right - Glyph.Rect.Left), static_cast<f32>(Glyph.Rect.Bottom - Glyph.Rect.Top), 0.0f
    );
    
    VertexData[0].TexCoord = dim::point2df(Mapping.Left, Mapping.Top);
    VertexData[1].TexCoord = dim::point2df(Mapping.Right, Mapping.Top);
    VertexData[2].TexCoord = dim::point2df(Mapping.Left, Mapping.Bottom);
    VertexData[3].TexCoord = dim::point2df(Mapping.Right, Mapping.Bottom);
    
    VertexData += 4;
    
    RawVertexData = VertexData;
}

void Direct3D9RenderSystem::bindDrawingColor(const video::color &Color)
{
    D3DDevice_->SetRenderState(D3DRS_TEXTUREFACTOR, Color.getSingle());
    D3DDevice_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    D3DDevice_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
}
void Direct3D9RenderSystem::unbindDrawingColor()
{
    D3DDevice_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    D3DDevice_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}


/*
 * SResourceManagement structure
 */

Direct3D9RenderSystem::SResourceManagement::SResourceManagement()
{
}
Direct3D9RenderSystem::SResourceManagement::~SResourceManagement()
{
}

void Direct3D9RenderSystem::SResourceManagement::releaseAll()
{
    release(VertexBuffers   );
    release(IndexBuffers    );
    release(TextureResources);
    release(Queries         );
}


} // /namespace video

} // /namespace sp


#endif



// ================================================================================
