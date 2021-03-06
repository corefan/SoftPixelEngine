/*
 * Direct3D11 shader file
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

#include "RenderSystem/Direct3D11/spDirect3D11Shader.hpp"

#if defined(SP_COMPILE_WITH_DIRECT3D11)


#include "RenderSystem/Direct3D11/spDirect3D11RenderSystem.hpp"
#include "RenderSystem/Direct3D11/spDirect3D11ConstantBuffer.hpp"
#include "Platform/spSoftPixelDeviceOS.hpp"


namespace sp
{

extern video::RenderSystem* GlbRenderSys;

namespace video
{


/*
 * Internal members
 */

//extern const c8* const d3dVertexShaderVersions[];
//extern const c8* const d3dPixelShaderVersions[];

const c8* const d3dVertexShaderVersions[] =
{
    "vs_1_0",
    "vs_2_0",
    "vs_2_a",
    "vs_3_0",
    "vs_4_0",
    "vs_4_1",
    "vs_5_0",
    0
};

const c8* const d3dPixelShaderVersions[] =
{
    "ps_1_0",
    "ps_1_1",
    "ps_1_2",
    "ps_1_3",
    "ps_1_4",
    "ps_2_0",
    "ps_2_a",
    "ps_2_b",
    "ps_3_0",
    "ps_4_0",
    "ps_4_1",
    "ps_5_0",
    0
};

const c8* const d3dGeometryShaderVersions[] =
{
    "gs_4_0",
    "gs_4_1",
    "gs_5_0",
    0
};

const c8* const d3dComputeShaderVersions[] =
{
    "cs_4_0",
    "cs_4_1",
    "cs_5_0",
    0
};

const c8* const d3dHullShaderVersions[] =
{
    "hs_5_0",
    0
};

const c8* const d3dDomainShaderVersions[] =
{
    "ds_5_0",
    0
};


/*
 * Direct3D11Shader class
 */

Direct3D11Shader::Direct3D11Shader(ShaderClass* ShdClass, const EShaderTypes Type, const EShaderVersions Version) :
    Shader              (ShdClass, Type, Version),
    D3DDevice_          (0                      ),
    D3DDeviceContext_   (0                      ),
    VSObj_              (0                      ),
    InputVertexLayout_  (0                      ),
    ShaderReflection_   (0                      )
{
    D3DDevice_          = static_cast<video::Direct3D11RenderSystem*>(GlbRenderSys)->D3DDevice_;
    D3DDeviceContext_   = static_cast<video::Direct3D11RenderSystem*>(GlbRenderSys)->D3DDeviceContext_;
    
    if (!ShdClass_)
        ShdClass_ = new Direct3D11ShaderClass();
    
    updateShaderClass();
}
Direct3D11Shader::~Direct3D11Shader()
{
    MemoryManager::deleteList(ConstantBufferList_);

    switch (Type_)
    {
        case SHADER_VERTEX:
            Direct3D11RenderSystem::releaseObject(VSObj_);
            break;
        case SHADER_PIXEL:
            Direct3D11RenderSystem::releaseObject(PSObj_);
            break;
        case SHADER_GEOMETRY:
            Direct3D11RenderSystem::releaseObject(GSObj_);
            break;
        case SHADER_HULL:
            Direct3D11RenderSystem::releaseObject(HSObj_);
            break;
        case SHADER_DOMAIN:
            Direct3D11RenderSystem::releaseObject(DSObj_);
            break;
        case SHADER_COMPUTE:
            Direct3D11RenderSystem::releaseObject(CSObj_);
            break;
    }
    
    Direct3D11RenderSystem::releaseObject(InputVertexLayout_);
}


/* === Shader compilation === */

bool Direct3D11Shader::compile(
    const std::list<io::stringc> &ShaderBuffer, const io::stringc &EntryPoint, const c8** CompilerOptions, u32 Flags)
{
    bool Result = false;
    
    c8* ProgramBuffer = 0;
    Shader::createProgramString(ShaderBuffer, ProgramBuffer);
    
    const c8* TargetName = 0;
    
    switch (Type_)
    {
        case SHADER_VERTEX:     TargetName = d3dVertexShaderVersions    [getVersionIndex(HLSL_VERTEX_1_0,   HLSL_VERTEX_5_0     )]; break;
        case SHADER_PIXEL:      TargetName = d3dPixelShaderVersions     [getVersionIndex(HLSL_PIXEL_1_0,    HLSL_PIXEL_5_0      )]; break;
        case SHADER_GEOMETRY:   TargetName = d3dGeometryShaderVersions  [getVersionIndex(HLSL_GEOMETRY_4_0, HLSL_GEOMETRY_5_0   )]; break;
        case SHADER_HULL:       TargetName = d3dHullShaderVersions      [getVersionIndex(HLSL_HULL_5_0,     HLSL_HULL_5_0       )]; break;
        case SHADER_DOMAIN:     TargetName = d3dDomainShaderVersions    [getVersionIndex(HLSL_DOMAIN_5_0,   HLSL_DOMAIN_5_0     )]; break;
        case SHADER_COMPUTE:    TargetName = d3dComputeShaderVersions   [getVersionIndex(HLSL_COMPUTE_4_0,  HLSL_COMPUTE_5_0    )]; break;
        default:
            break;
    }
    
    if (TargetName)
    {
        Result = compileHLSL(ProgramBuffer, EntryPoint.c_str(), TargetName, Flags);
        
        if (Result)
            createConstantBuffers();
    }
    else
        io::Log::error("Invalid target profile for D3D11 shader");
    
    delete [] ProgramBuffer;
    
    CompiledSuccessfully_ = Result;
    
    return CompiledSuccessfully_;
}


/* === Set the constant buffer === */

bool Direct3D11Shader::setConstantBuffer(const io::stringc &Name, const void* Buffer)
{
    ConstantBuffer* ConstBuf = getConstantBuffer(Name);
    return ConstBuf ? ConstBuf->updateBuffer(Buffer) : false;
}

bool Direct3D11Shader::setConstantBuffer(u32 Number, const void* Buffer)
{
    return Number < ConstantBufferList_.size() ? ConstantBufferList_[Number]->updateBuffer(Buffer) : false;
}

u32 Direct3D11Shader::getConstantCount() const
{
    return 0; // !TODO!
}

std::vector<io::stringc> Direct3D11Shader::getConstantList() const
{
    return std::vector<io::stringc>(); // !TODO!
}


/*
 * ======= Private: =======
 */

bool Direct3D11Shader::compileHLSL(
    const c8* ProgramBuffer, const c8* EntryPoint, const c8* TargetName, u32 Flags)
{
    if (!ProgramBuffer)
        return false;
    
    /* Temporary variables */
    ID3DBlob* Buffer = 0;
    ID3DBlob* Errors = 0;
    
    /* Get the shader name */
    const io::stringc ShaderName = getDescription();
    
    /* Compile the shader */
    HRESULT Result = D3DCompile(
        ProgramBuffer,              /* Shader source */
        strlen(ProgramBuffer),      /* Source length */
        0,                          /* Source name */
        0,                          /* Shader macros */
        0,                          /* Include file handeing */
        EntryPoint,                 /* Entry point (shader's main function) */
        TargetName,                 /* Target name (shader's version) */
        getCompilerFlags(Flags),    /* Compile flags (flags1) */
        0,                          /* Effect flags (flags2) */
        &Buffer,                    /* Compiled output shader code */
        &Errors                     /* Error messages */
    );
    
    /* Check for errors */
    if (Result)
    {
        io::Log::message("Direct3D11 HLSL " + ShaderName + " shader compilation failed:", io::LOG_ERROR);
        
        if (Errors)
        {
            io::Log::message((c8*)Errors->GetBufferPointer(), io::LOG_ERROR);
            Errors->Release();
        }
        
        if (Buffer)
            Buffer->Release();
        
        return false;
    }
    
    if (!Buffer)
        return false;
    
    /* Create the vertex shader */
    switch (Type_)
    {
        case SHADER_VERTEX:
            Result = D3DDevice_->CreateVertexShader     (Buffer->GetBufferPointer(), Buffer->GetBufferSize(), 0, &VSObj_);
            break;
        case SHADER_PIXEL:
            Result = D3DDevice_->CreatePixelShader      (Buffer->GetBufferPointer(), Buffer->GetBufferSize(), 0, &PSObj_);
            break;
        case SHADER_GEOMETRY:
            Result = D3DDevice_->CreateGeometryShader   (Buffer->GetBufferPointer(), Buffer->GetBufferSize(), 0, &GSObj_);
            break;
        case SHADER_HULL:
            Result = D3DDevice_->CreateHullShader       (Buffer->GetBufferPointer(), Buffer->GetBufferSize(), 0, &HSObj_);
            break;
        case SHADER_DOMAIN:
            Result = D3DDevice_->CreateDomainShader     (Buffer->GetBufferPointer(), Buffer->GetBufferSize(), 0, &DSObj_);
            break;
        case SHADER_COMPUTE:
            Result = D3DDevice_->CreateComputeShader    (Buffer->GetBufferPointer(), Buffer->GetBufferSize(), 0, &CSObj_);
            break;
    }
    
    if (Result)
    {
        io::Log::error("Could not create HLSL " + ShaderName + " shader");
        Buffer->Release();
        return false;
    }
    
    if (Type_ == SHADER_VERTEX)
    {
        /* Get vertex input layout description */
        const std::vector<D3D11_INPUT_ELEMENT_DESC>* InputDesc = static_cast<std::vector<D3D11_INPUT_ELEMENT_DESC>*>(
            static_cast<Direct3D11ShaderClass*>(ShdClass_)->VertexFormat_->InputLayout_
        );
        
        if (InputDesc)
        {
            /* Create the vertex layout */
            Result = D3DDevice_->CreateInputLayout(
                &(*InputDesc)[0], InputDesc->size(),
                Buffer->GetBufferPointer(), Buffer->GetBufferSize(), &InputVertexLayout_
            );
            
            if (Result)
            {
                io::Log::error("Could not create vertex input layout");
                Buffer->Release();
                return false;
            }
        }
    }
    
    /* Get the shader reflection */
    if (D3DReflect(Buffer->GetBufferPointer(), Buffer->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&ShaderReflection_))
    {
        io::Log::error("Could not get shader reflection");
        Buffer->Release();
        return false;
    }
    
    Buffer->Release();
    
    return true;
}

bool Direct3D11Shader::createConstantBuffers()
{
    if (!ShaderReflection_)
        return false;
    
    /* Temporary variables */
    ID3D11ShaderReflectionConstantBuffer* ReflectionBuffer = 0;
    
    D3D11_SHADER_DESC ShaderDesc;
    D3D11_SHADER_BUFFER_DESC ShaderBufferDesc;
    //D3D11_SHADER_INPUT_BIND_DESC ShaderResourceDesc;
    
    D3D11_BUFFER_DESC BufferDesc;
    ZeroMemory(&BufferDesc, sizeof(D3D11_BUFFER_DESC));
    
    /* Release and clear old constant buffers */
    MemoryManager::deleteList(ConstantBufferList_);
    HWConstantBuffers_.clear();
    
    /* Examine each shader constant buffer */
    ShaderReflection_->GetDesc(&ShaderDesc);
    
    /* Create each constant buffer */
    for (u32 i = 0; i < ShaderDesc.ConstantBuffers; ++i)
    {
        /* Get shader buffer description */
        ReflectionBuffer = ShaderReflection_->GetConstantBufferByIndex(i);
        
        if (!ReflectionBuffer || ReflectionBuffer->GetDesc(&ShaderBufferDesc))
        {
            io::Log::error("Could not reflect constant buffer #" + io::stringc(i));
            return false;
        }
        
        if (ShaderBufferDesc.Type != D3D11_CT_CBUFFER)
            continue;
        
        /* Create the constant buffer */
        Direct3D11ConstantBuffer* NewConstBuffer = new Direct3D11ConstantBuffer(
            static_cast<Direct3D11ShaderClass*>(ShdClass_), ShaderBufferDesc, i
        );

        if (!NewConstBuffer->valid())
        {
            delete NewConstBuffer;
            return false;
        }

        HWConstantBuffers_.push_back(NewConstBuffer->getBufferRef());
        ConstantBufferList_.push_back(NewConstBuffer);
    }

    /* Create each buffer resource */
    /*for (u32 i = 0; i < ShaderDesc.BoundResources; ++i)
    {
        if (ShaderReflection_->GetResourceBindingDesc(i, &ShaderResourceDesc) != S_OK)
        {
            io::Log::error("Could not reflect shader resource #" + io::stringc(i));
            return false;
        }


    }*/
    
    return true;
}

UINT Direct3D11Shader::getCompilerFlags(u32 Flags) const
{
    UINT CompilerFlags = 0;
    
    if (Flags & SHADERFLAG_NO_OPTIMIZATION)
        CompilerFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
    else
        CompilerFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
    
    if (Flags & COMPILE_SHADER_NO_VALIDATION)
        CompilerFlags |= D3DCOMPILE_SKIP_VALIDATION;
    
    if (Flags & COMPILE_SHADER_AVOID_FLOW_CONTROL)
        CompilerFlags |= D3DCOMPILE_AVOID_FLOW_CONTROL;
    else if (Flags & COMPILE_SHADER_PREFER_FLOW_CONTROL)
        CompilerFlags |= D3DCOMPILE_PREFER_FLOW_CONTROL;
    
    return CompilerFlags;
}


} // /namespace scene

} // /namespace sp


#endif



// ================================================================================
