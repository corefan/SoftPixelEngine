/*
 * Material script reader file
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

#include "Framework/Tools/ScriptParser/spUtilityMaterialScriptReader.hpp"

#ifdef SP_COMPILE_WITH_MATERIAL_SCRIPT


#include "Base/spBaseExceptions.hpp"
#include "Base/spVertexFormatUniversal.hpp"
#include "Base/spTimer.hpp"
#include "RenderSystem/spShaderClass.hpp"
#include "RenderSystem/spRenderSystem.hpp"
#include "RenderSystem/spTextureLayerStandard.hpp"
#include "RenderSystem/spTextureLayerRelief.hpp"
#include "Platform/spSoftPixelDevice.hpp"

#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

//#include <iterator>
//#include <algorithm>

namespace sp
{

extern SoftPixelDevice* GlbEngineDev;
extern video::RenderSystem* GlbRenderSys;

namespace tool
{



/*
 * Internal structures
 */

template <typename T> struct SHashMapContainer
{
    SHashMapContainer()
    {
    }
    ~SHashMapContainer()
    {
    }

    /* Functions */
    const T& find(const std::string &Key, const T &Default, const io::stringc &Err) const
    {
        typename std::map<std::string, T>::const_iterator it = HashMap.find(Key);

        if (it != HashMap.end())
            return it->second;

        io::Log::warning(Err);

        return Default;
    }

    /* Operators */
    //! Inserts a new hash-map entry.
    SHashMapContainer<T>& operator () (const std::string &Key, const T &Value)
    {
        HashMap[Key] = Value;
        return *this;
    }

    /* Members */
    std::map<std::string, T> HashMap;
};


/*
 * Internal functions
 */

#define DEFINE_HASHMAP_SETUP_PROC(t, n, c)                          \
    static SHashMapContainer<t> StcSetupHashMap##n()                \
    {                                                               \
        SHashMapContainer<t> HashMap;                               \
        HashMap c;                                                  \
        return HashMap;                                             \
    }                                                               \
    static SHashMapContainer<t> HashMap##n = StcSetupHashMap##n();

DEFINE_HASHMAP_SETUP_PROC(
    video::EShadingTypes, EShadingTypes,
        ("flat",        video::SHADING_FLAT     )
        ("gouraud",     video::SHADING_GOURAUD  )
        ("phong",       video::SHADING_PHONG    )
        ("perPixel",    video::SHADING_PERPIXEL )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::ESizeComparisionTypes, ESizeComparisionTypes,
        ("never",           video::CMPSIZE_NEVER        )
        ("equal",           video::CMPSIZE_EQUAL        )
        ("notEqual",        video::CMPSIZE_NOTEQUAL     )
        ("less",            video::CMPSIZE_LESS         )
        ("lessEqual",       video::CMPSIZE_LESSEQUAL    )
        ("greater",         video::CMPSIZE_GREATER      )
        ("greaterEqual",    video::CMPSIZE_GREATEREQUAL )
        ("always",          video::CMPSIZE_ALWAYS       )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::EBlendingTypes, EBlendingTypes,
        ("zero",            video::BLEND_ZERO           )
        ("one",             video::BLEND_ONE            )
        ("srcColor",        video::BLEND_SRCCOLOR       )
        ("invSrcColor",     video::BLEND_INVSRCCOLOR    )
        ("srcAlpha",        video::BLEND_SRCALPHA       )
        ("invSrcAlpha",     video::BLEND_INVSRCALPHA    )
        ("destColor",       video::BLEND_DESTCOLOR      )
        ("invDestColor",    video::BLEND_INVDESTCOLOR   )
        ("destAlpha",       video::BLEND_DESTALPHA      )
        ("invDestAlpha",    video::BLEND_INVDESTALPHA   )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::EWireframeTypes, EWireframeTypes,
        ("points",  video::WIREFRAME_POINTS )
        ("lines",   video::WIREFRAME_LINES  )
        ("solid",   video::WIREFRAME_SOLID  )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::EFaceTypes, EFaceTypes,
        ("front",   video::FACE_FRONT   )
        ("back",    video::FACE_BACK    )
        ("both",    video::FACE_BOTH    )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::EShaderTypes, EShaderTypes,
        ("vertexAsm",   video::SHADER_VERTEX_PROGRAM)
        ("pixelAsm",    video::SHADER_PIXEL_PROGRAM )
        ("vertex",      video::SHADER_VERTEX        )
        ("pixel",       video::SHADER_PIXEL         )
        ("geometry",    video::SHADER_GEOMETRY      )
        ("hull",        video::SHADER_HULL          )
        ("domain",      video::SHADER_DOMAIN        )
        ("compute",     video::SHADER_COMPUTE       )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::ERendererDataTypes, ERendererDataTypes,
        ("float",   video::DATATYPE_FLOAT           )
        ("double",  video::DATATYPE_DOUBLE          )
        ("byte",    video::DATATYPE_BYTE            )
        ("short",   video::DATATYPE_SHORT           )
        ("int",     video::DATATYPE_INT             )
        ("ubyte",   video::DATATYPE_UNSIGNED_BYTE   )
        ("ushort",  video::DATATYPE_UNSIGNED_SHORT  )
        ("uint",    video::DATATYPE_UNSIGNED_INT    )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::EVertexFormatFlags, EVertexFormatFlags,
        ("coord",       video::VERTEXFORMAT_COORD       )
        ("color",       video::VERTEXFORMAT_COLOR       )
        ("normal",      video::VERTEXFORMAT_NORMAL      )
        ("binormal",    video::VERTEXFORMAT_BINORMAL    )
        ("tangent",     video::VERTEXFORMAT_TANGENT     )
        ("fogCoord",    video::VERTEXFORMAT_FOGCOORD    )
        ("texCoord",    video::VERTEXFORMAT_TEXCOORDS   )
        ("universal",   video::VERTEXFORMAT_UNIVERSAL   )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::ETextureTypes, ETextureTypes,
        ("tex1D",           video::TEXTURE_1D           )
        ("tex2D",           video::TEXTURE_2D           )
        ("tex3D",           video::TEXTURE_3D           )
        ("texCube",         video::TEXTURE_CUBEMAP      )
        ("tex1DArray",      video::TEXTURE_1D_ARRAY     )
        ("tex2DArray",      video::TEXTURE_2D_ARRAY     )
        ("texCubeArray",    video::TEXTURE_CUBEMAP_ARRAY)
        ("texRect",         video::TEXTURE_RECTANGLE    )
        ("texBuffer",       video::TEXTURE_BUFFER       )
        ("tex1DRW",         video::TEXTURE_1D_RW        )
        ("tex2DRW",         video::TEXTURE_2D_RW        )
        ("tex3DRW",         video::TEXTURE_3D_RW        )
        ("tex1DArrayRW",    video::TEXTURE_1D_ARRAY_RW  )
        ("tex2DArrayRW",    video::TEXTURE_2D_ARRAY_RW  )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::EImageBufferTypes, EImageBufferTypes,
        ("ubyte",   video::IMAGEBUFFER_UBYTE)
        ("float",   video::IMAGEBUFFER_FLOAT)
)

DEFINE_HASHMAP_SETUP_PROC(
    video::EPixelFormats, EPixelFormats,
        ("alpha",           video::PIXELFORMAT_ALPHA    )
        ("gray",            video::PIXELFORMAT_GRAY     )
        ("grayAlpha",       video::PIXELFORMAT_GRAYALPHA)
        ("rgb",             video::PIXELFORMAT_RGB      )
        ("bgr",             video::PIXELFORMAT_BGR      )
        ("rgba",            video::PIXELFORMAT_RGBA     )
        ("bgra",            video::PIXELFORMAT_BGRA     )
        ("depthComponent",  video::PIXELFORMAT_DEPTH    )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::EHWTextureFormats, EHWTextureFormats,
        ("ubyte8",  video::HWTEXFORMAT_UBYTE8   )
        ("float16", video::HWTEXFORMAT_FLOAT16  )
        ("float32", video::HWTEXFORMAT_FLOAT32  )
        ("int32",   video::HWTEXFORMAT_INT32    )
        ("uint32",  video::HWTEXFORMAT_UINT32   )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::ETextureWrapModes, ETextureWrapModes,
        ("repeat",  video::TEXWRAP_REPEAT   )
        ("mirror",  video::TEXWRAP_MIRROR   )
        ("clamp",   video::TEXWRAP_CLAMP    )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::ETextureFilters, ETextureFilters,
        ("linear",  video::FILTER_LINEAR)
        ("smooth",  video::FILTER_SMOOTH)
)

DEFINE_HASHMAP_SETUP_PROC(
    video::ETextureMipMapFilters, ETextureMipMapFilters,
        ("bilinear",    video::FILTER_BILINEAR      )
        ("trilinear",   video::FILTER_TRILINEAR     )
        ("anisotropic", video::FILTER_ANISOTROPIC   )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::ETextureEnvTypes, ETextureEnvTypes,
        ("modulate",    video::TEXENV_MODULATE      )
        ("replace",     video::TEXENV_REPLACE       )
        ("add",         video::TEXENV_ADD           )
        ("addSigned",   video::TEXENV_ADDSIGNED     )
        ("subtract",    video::TEXENV_SUBTRACT      )
        ("interpolate", video::TEXENV_INTERPOLATE   )
        ("dot3",        video::TEXENV_DOT3          )
)

DEFINE_HASHMAP_SETUP_PROC(
    video::EMappingGenTypes, EMappingGenTypes,
        ("disable",         video::MAPGEN_DISABLE       )
        ("objectLinear",    video::MAPGEN_OBJECT_LINEAR )
        ("eyeLinear",       video::MAPGEN_EYE_LINEAR    )
        ("sphereMap",       video::MAPGEN_SPHERE_MAP    )
        ("normalMap",       video::MAPGEN_NORMAL_MAP    )
        ("reflectionMap",   video::MAPGEN_REFLECTION_MAP)
)

DEFINE_HASHMAP_SETUP_PROC(
    scene::EAnimPlaybackModes, EAnimPlaybackModes,
        ("oneShot",         scene::PLAYBACK_ONESHOT         )
        ("oneLoop",         scene::PLAYBACK_ONELOOP         )
        ("loop",            scene::PLAYBACK_LOOP            )
        ("pingPong",        scene::PLAYBACK_PINGPONG        )
        ("pingPongLoop",    scene::PLAYBACK_PINGPONG_LOOP   )
)

#undef DEFINE_HASHMAP_SETUP_PROC


/*
 * Internal macros
 */

#define PARSE_ENUM(n) MaterialScriptReader::n(readIdentifier())

#define READ_SCRIPT_BLOCK(f)                \
    while (1)                               \
    {                                       \
        nextTokenNoEOF();                   \
                                            \
        if (type() == TOKEN_BRACE_RIGHT)    \
            break;                          \
        else if (type() == TOKEN_NAME)      \
        {                                   \
            if (Tkn_->Str == "discard")     \
                ignoreNextBlock();          \
            else                            \
            {                               \
                f                           \
            }                               \
        }                                   \
        else                                \
            readVarDefinition();            \
    }


/*
 * MaterialScriptReader class
 */

MaterialScriptReader::MaterialScriptReader() :
    ScriptReaderBase    (                           ),
    Materials_          (video::MaterialStatesPtr() ),
    Shaders_            (0                          ),
    VertexFormats_      (0                          ),
    Textures_           (0                          ),
    TexLayers_          (video::TextureLayerPtr()   ),
    CurShaderVersion_   (video::DUMMYSHADER_VERSION ),
    CurTexRenderTarget_ (false                      )
{
}
MaterialScriptReader::~MaterialScriptReader()
{
}

bool MaterialScriptReader::loadScript(const io::stringc &Filename)
{
    io::Log::message("Load material script: \"" + Filename + "\"");
    io::Log::ScopedTab Unused;
    
    /* Reset internal states */
    Materials_      .reset(video::MaterialStatesPtr());
    Shaders_        .reset(0);
    VertexFormats_  .reset(0);
    Textures_       .reset(0);
    
    clearVariables();
    
    /* Read file into string */
    io::stringc InputScript;
    if (!io::FileSystem().readFileString(Filename, InputScript))
        return false;
    
    /* Parse tokens from input shader code */
    TokenStream_ = Scanner_.readTokens(InputScript.c_str(), COMMENTSTYLE_BASIC);
    
    if (!TokenStream_)
        return exitWithError("Invalid token iterator");
    
    /* Validate brackets */
    if (!validateBrackets())
        return false;
    
    /* Define all default variables */
    defineDefaultVariables();
    
    /* Iterate over all tokens */
    bool Result = true;
    
    try
    {
        while (nextToken())
        {
            if (type() == TOKEN_NAME)
            {
                if (Tkn_->Str == "discard")
                    ignoreNextBlock();
                else
                    readScriptBlock();
            }
            else
                readVarDefinition();
        }
        
        printInfo();
    }
    catch (const std::exception &Err)
    {
        Result = exitWithError(Err.what());
    }
    
    return Result;
}

video::MaterialStatesPtr MaterialScriptReader::findMaterial(const io::stringc &Name)
{
    return Materials_.find(Name, video::MaterialStatesPtr());
}

video::ShaderClass* MaterialScriptReader::findShader(const io::stringc &Name)
{
    return Shaders_.find(Name, 0);
}

video::VertexFormatUniversal* MaterialScriptReader::findVertexFormat(const io::stringc &Name)
{
    return VertexFormats_.find(Name, 0);
}

video::Texture* MaterialScriptReader::findTexture(const io::stringc &Name)
{
    return Textures_.find(Name, 0);
}

video::TextureLayerPtr MaterialScriptReader::findTextureLayer(const io::stringc &Name)
{
    return TexLayers_.find(Name, video::TextureLayerPtr());
}

bool MaterialScriptReader::defineString(const io::stringc &VariableName, const io::stringc &Str)
{
    if (isVariableFree(VariableName))
    {
        registerString(VariableName, Str);
        return true;
    }
    return false;
}

bool MaterialScriptReader::defineNumber(const io::stringc &VariableName, f64 Number)
{
    if (isVariableFree(VariableName))
    {
        registerNumber(VariableName, Number);
        return true;
    }
    return false;
}

const video::VertexFormat* MaterialScriptReader::parseVertexFormat(const io::stringc &FormatName) const
{
    if (FormatName.empty())
        return 0;
    
    /* Search for pre-defined vertex formats */
         if (FormatName == "vertexFormatDefault"    ) return GlbRenderSys->getVertexFormatDefault   ();
    else if (FormatName == "vertexFormatReduced"    ) return GlbRenderSys->getVertexFormatReduced   ();
    else if (FormatName == "vertexFormatExtended"   ) return GlbRenderSys->getVertexFormatExtended  ();
    else if (FormatName == "vertexFormatFull"       ) return GlbRenderSys->getVertexFormatFull      ();
    
    /* Search for user-defined vertex formats */
    const video::VertexFormatUniversal* VertFmt = VertexFormats_.find(FormatName, 0);
    
    if (VertFmt)
        return VertFmt;
    
    io::Log::warning("Unknown vertex format \"" + FormatName + "\"");
    
    return 0;
}

video::EShadingTypes MaterialScriptReader::parseShading(const io::stringc &Identifier)
{
    return HashMapEShadingTypes.find(
        Identifier.str(), video::SHADING_FLAT, "Unknown shading type \"" + Identifier + "\""
    );
}

video::ESizeComparisionTypes MaterialScriptReader::parseCompareType(const io::stringc &Identifier)
{
    return HashMapESizeComparisionTypes.find(
        Identifier.str(), video::CMPSIZE_NEVER, "Unknown size compare type \"" + Identifier + "\""
    );
}

video::EBlendingTypes MaterialScriptReader::parseBlendType(const io::stringc &Identifier)
{
    return HashMapEBlendingTypes.find(
        Identifier.str(), video::BLEND_ZERO, "Unknown blend type \"" + Identifier + "\""
    );
}

video::EWireframeTypes MaterialScriptReader::parseWireframe(const io::stringc &Identifier)
{
    return HashMapEWireframeTypes.find(
        Identifier.str(), video::WIREFRAME_POINTS, "Unknown wireframe type \"" + Identifier + "\""
    );
}

video::EFaceTypes MaterialScriptReader::parseFaceType(const io::stringc &Identifier)
{
    return HashMapEFaceTypes.find(
        Identifier.str(), video::FACE_FRONT, "Unknown face type \"" + Identifier + "\""
    );
}

video::EShaderTypes MaterialScriptReader::parseShaderType(const io::stringc &Identifier)
{
    return HashMapEShaderTypes.find(
        Identifier.str(), video::SHADER_DUMMY, "Unknown shader type \"" + Identifier + "\""
    );
}

video::EShaderVersions MaterialScriptReader::parseShaderVersion(const io::stringc &Identifier)
{
    static const c8* VerListGLSL[] = { "std120", "std130", "std140", "std150", "std330", "std400", "std410", "std420", "std430", 0 };
    static const c8* VerListDXVS[] = { "vs_1_0", "vs_2_0", "vs_2_a", "vs_3_0", "vs_4_0", "vs_4_1", "vs_5_0", 0 };
    static const c8* VerListDXPS[] = { "ps_1_0", "ps_1_1", "ps_1_2", "ps_1_3", "ps_1_4", "ps_2_0", "ps_2_a", "ps_2_b", "ps_3_0", "ps_4_0", "ps_4_1", "ps_5_0", 0 };
    static const c8* VerListDXGS[] = { "gs_4_0", "gs_4_1", "gs_5_0", 0 };
    static const c8* VerListDXCS[] = { "cs_4_0", "cs_4_1", "cs_5_0", 0 };
    static const c8* VerListDXHS[] = { "hs_5_0", 0 };
    static const c8* VerListDXDS[] = { "ds_5_0", 0 };
    static const c8* VerListCg  [] = { "cg_2_0", 0 };
    
    if (Identifier.size() == 6)
    {
        const c8** Ver = 0;
        u32 i = 0;
        
        /* Get shader version type */
        const c8 Type = Identifier[0];
        
        switch (Type)
        {
            case 's':
                Ver = VerListGLSL;
                i = static_cast<u32>(video::GLSL_VERSION_1_20);
                break;
            case 'v':
                Ver = VerListDXVS;
                i = static_cast<u32>(video::HLSL_VERTEX_1_0);
                break;
            case 'p':
                Ver = VerListDXPS;
                i = static_cast<u32>(video::HLSL_PIXEL_1_0);
                break;
            case 'g':
                Ver = VerListDXGS;
                i = static_cast<u32>(video::HLSL_GEOMETRY_4_0);
                break;
            case 'c':
                if (Identifier[1] == 's')
                {
                    Ver = VerListDXCS;
                    i = static_cast<u32>(video::HLSL_COMPUTE_4_0);
                }
                else
                {
                    Ver = VerListCg;
                    i = static_cast<u32>(video::CG_VERSION_2_0);
                }
                break;
            case 'h':
                Ver = VerListDXHS;
                i = static_cast<u32>(video::HLSL_HULL_5_0);
                break;
            case 'd':
                Ver = VerListDXDS;
                i = static_cast<u32>(video::HLSL_DOMAIN_5_0);
                break;
        }
        
        /* Search for shader version */
        if (Ver)
        {
            while (*Ver)
            {
                if (Identifier == *Ver)
                    return static_cast<video::EShaderVersions>(i);
                ++Ver;
                ++i;
            }
        }
    }
    
    io::Log::warning("Unknown shader version \"" + Identifier + "\"");
    
    return video::DUMMYSHADER_VERSION;
}

video::ERendererDataTypes MaterialScriptReader::parseDataType(const io::stringc &Identifier)
{
    return HashMapERendererDataTypes.find(
        Identifier.str(), video::DATATYPE_FLOAT, "Unknown data type \"" + Identifier + "\""
    );
}

video::EVertexFormatFlags MaterialScriptReader::parseFormatFlag(const io::stringc &Identifier)
{
    return HashMapEVertexFormatFlags.find(
        Identifier.str(), video::VERTEXFORMAT_UNIVERSAL, "Unknown vertex flag \"" + Identifier + "\""
    );
}

video::ETextureTypes MaterialScriptReader::parseTextureType(const io::stringc &Identifier)
{
    return HashMapETextureTypes.find(
        Identifier.str(), video::TEXTURE_2D, "Unknown texture type \"" + Identifier + "\""
    );
}

video::EImageBufferTypes MaterialScriptReader::parseBufferType(const io::stringc &Identifier)
{
    return HashMapEImageBufferTypes.find(
        Identifier.str(), video::IMAGEBUFFER_UBYTE, "Unknown image buffer type \"" + Identifier + "\""
    );
}

video::EPixelFormats MaterialScriptReader::parsePixelFormat(const io::stringc &Identifier)
{
    return HashMapEPixelFormats.find(
        Identifier.str(), video::PIXELFORMAT_RGBA, "Unknown pixel format \"" + Identifier + "\""
    );
}

video::EHWTextureFormats MaterialScriptReader::parseHWTexFormat(const io::stringc &Identifier)
{
    return HashMapEHWTextureFormats.find(
        Identifier.str(), video::HWTEXFORMAT_UBYTE8, "Unknown hardware texture format \"" + Identifier + "\""
    );
}

video::ETextureWrapModes MaterialScriptReader::parseTexWrapMode(const io::stringc &Identifier)
{
    return HashMapETextureWrapModes.find(
        Identifier.str(), video::TEXWRAP_REPEAT, "Unknown texture wrap mode \"" + Identifier + "\""
    );
}

video::ETextureFilters MaterialScriptReader::parseTexFilter(const io::stringc &Identifier)
{
    return HashMapETextureFilters.find(
        Identifier.str(), video::FILTER_SMOOTH, "Unknown texture filter \"" + Identifier + "\""
    );
}

video::ETextureMipMapFilters MaterialScriptReader::parseMIPMapFilter(const io::stringc &Identifier)
{
    return HashMapETextureMipMapFilters.find(
        Identifier.str(), video::FILTER_TRILINEAR, "Unknown texture filter \"" + Identifier + "\""
    );
}

video::ETextureEnvTypes MaterialScriptReader::parseTextureEnv(const io::stringc &Identifier)
{
    return HashMapETextureEnvTypes.find(
        Identifier.str(), video::TEXENV_MODULATE, "Unknown texture environment type \"" + Identifier + "\""
    );
}

video::EMappingGenTypes MaterialScriptReader::parseMappingGen(const io::stringc &Identifier)
{
    return HashMapEMappingGenTypes.find(
        Identifier.str(), video::MAPGEN_DISABLE, "Unknown texture coordinates mapping generation \"" + Identifier + "\""
    );
}

scene::EAnimPlaybackModes MaterialScriptReader::parsePlaybackModes(const io::stringc &Identifier)
{
    return HashMapEAnimPlaybackModes.find(
        Identifier.str(), scene::PLAYBACK_LOOP, "Unknown animation playback mode \"" + Identifier + "\""
    );
}


/*
 * ======= Protected: ========
 */

void MaterialScriptReader::printUnknownVar(const io::stringc &VariableName) const
{
    io::Log::warning("Unknown variable named \"" + VariableName + "\"");
}

void MaterialScriptReader::printInfo()
{
    io::stringc Info;
    
    Materials_      .appendInfo(Info, "Material", false );
    Shaders_        .appendInfo(Info, "Shader"          );
    VertexFormats_  .appendInfo(Info, "Vertex Format"   );
    Textures_       .appendInfo(Info, "Texture"         );
    TexLayers_      .appendInfo(Info, "Texture Layer"   );
    
    if (!Info.empty())
        io::Log::message("Created " + Info);
}

bool MaterialScriptReader::hasVariable(const io::stringc &VariableName) const
{
    /* Check if variable is already registered */
    std::map<std::string, io::stringc>::const_iterator itStr = StringVariables_.find(VariableName.str());
    if (itStr != StringVariables_.end())
        return true;
    
    std::map<std::string, f64>::const_iterator itNum = NumericVariables_.find(VariableName.str());
    if (itNum != NumericVariables_.end())
        return true;
    
    return false;
}

bool MaterialScriptReader::isVariableFree(const io::stringc &VariableName) const
{
    if (hasVariable(VariableName))
    {
        io::Log::error("Variable \"" + VariableName + "\" already used in material script");
        return false;
    }
    return true;
}

void MaterialScriptReader::registerString(const io::stringc &VariableName, const io::stringc &Str)
{
    StringVariables_[VariableName.str()] = Str;
}

void MaterialScriptReader::registerNumber(const io::stringc &VariableName, f64 Number)
{
    NumericVariables_[VariableName.str()] = Number;
}

bool MaterialScriptReader::getVarValue(const io::stringc &VariableName, io::stringc &StrVal, f64 &NumVal, bool &IsStr) const
{
    /* Search variable in string list */
    std::map<std::string, io::stringc>::const_iterator itStr = StringVariables_.find(VariableName.str());
    
    if (itStr != StringVariables_.end())
    {
        StrVal = itStr->second;
        IsStr = true;
        return true;
    }
    
    /* Search variable in number list */
    std::map<std::string, f64>::const_iterator itNum = NumericVariables_.find(VariableName.str());
    
    if (itNum != NumericVariables_.end())
    {
        NumVal = itNum->second;
        IsStr = false;
        return true;
    }
    
    /* Print warning */
    printUnknownVar(VariableName);
    
    return false;
}

io::stringc MaterialScriptReader::getString(const io::stringc &VariableName) const
{
    /* Find variable by name */
    std::map<std::string, io::stringc>::const_iterator it = StringVariables_.find(VariableName.str());
    if (it != StringVariables_.end())
        return it->second;
    
    /* Print warning and return default value */
    printUnknownVar(VariableName);
    return "";
}

f64 MaterialScriptReader::getNumber(const io::stringc &VariableName) const
{
    /* Find variable by name */
    std::map<std::string, f64>::const_iterator it = NumericVariables_.find(VariableName.str());
    if (it != NumericVariables_.end())
        return it->second;
    
    /* Print warning and return default value */
    printUnknownVar(VariableName);
    return 0.0;
}

void MaterialScriptReader::breakEOF()
{
    throw io::DefaultException("Unexpected end-of-file");
}

void MaterialScriptReader::breakUnexpectedToken()
{
    throw io::DefaultException("Unexpected token");
}

void MaterialScriptReader::breakUnexpectedIdentifier()
{
    throw io::DefaultException("Unexpected identifier named \"" + Tkn_->Str + "\"");
}

void MaterialScriptReader::breakExpectedIdentifier()
{
    throw io::DefaultException("Expected identifier");
}

void MaterialScriptReader::breakExpectedAssignment()
{
    throw io::DefaultException("Expected assignment character");
}

void MaterialScriptReader::breakExpectedString()
{
    throw io::DefaultException("Expected string");
}

void MaterialScriptReader::breakSingleNumberOnly()
{
    throw io::DefaultException("Only strings can be combined with '+' characters");
}

void MaterialScriptReader::breakStringCombination()
{
    throw io::DefaultException("Strings must be combined with a '+' character");
}

void MaterialScriptReader::nextTokenNoEOF(bool IgnoreWhiteSpaces)
{
    if (!nextToken(IgnoreWhiteSpaces))
        breakEOF();
}

void MaterialScriptReader::ignoreNextBlock()
{
    TokenStream_->ignoreBlock(true);
}

void MaterialScriptReader::addMaterial(const io::stringc &Name)
{
    Materials_.add(Name, boost::make_shared<video::MaterialStates>());
}

void MaterialScriptReader::addShader(const io::stringc &Name, const video::VertexFormat* InputLayout)
{
    Shaders_.add(Name, GlbRenderSys->createShaderClass(InputLayout));
}

void MaterialScriptReader::addVertexFormat(const io::stringc &Name)
{
    VertexFormats_.add(Name, GlbRenderSys->createVertexFormat<video::VertexFormatUniversal>());
}

void MaterialScriptReader::addTexture(const io::stringc &Name, video::Texture* Tex)
{
    Textures_.add(Name, Tex);
}

void MaterialScriptReader::addTextureLayer(const io::stringc &Name, const io::stringc &LayerType)
{
    /* Create texture layer */
    video::TextureLayerPtr Layer;
    
         if (LayerType == "base"    ) Layer = boost::make_shared<video::TextureLayer        >();
    else if (LayerType == "standard") Layer = boost::make_shared<video::TextureLayerStandard>();
    else if (LayerType == "relief"  ) Layer = boost::make_shared<video::TextureLayerRelief  >();
    else
        throw io::DefaultException("Unknown texture layer type named \"" + LayerType + "\"");
    
    /* Add texture layer to list */
    TexLayers_.add(Name, Layer);
}

void MaterialScriptReader::readMaterial()
{
    /* Read material name */
    nextTokenNoEOF();
    
    if (type() != TOKEN_STRING || Tkn_->Str.empty())
        breakExpectedIdentifier();
    
    const io::stringc& Name = Tkn_->Str;
    
    /* Check if material name already exists */
    if (findMaterial(Name) != 0)
        throw io::DefaultException("Multiple defintion of material named \"" + Name + "\"");
    
    /* Create new material */
    addMaterial(Name);
    
    /* Read material block */
    readBlockBegin();
    
    READ_SCRIPT_BLOCK(
        readMaterialState();
    )
}

void MaterialScriptReader::readMaterialState()
{
    const io::stringc& Name = Tkn_->Str;
    
         if (Name == "ambient"          ) Materials_.Current->setAmbientColor       (readColor()        );
    else if (Name == "diffuse"          ) Materials_.Current->setDiffuseColor       (readColor()        );
    else if (Name == "specular"         ) Materials_.Current->setSpecularColor      (readColor()        );
    else if (Name == "emission"         ) Materials_.Current->setEmissionColor      (readColor()        );
    
    else if (Name == "shininess"        ) Materials_.Current->setShininess          (readNumber<f32>()  );
    else if (Name == "offsetFactor"     ) Materials_.Current->setPolygonOffsetFactor(readNumber<f32>()  );
    else if (Name == "offsetUnits"      ) Materials_.Current->setPolygonOffsetUnits (readNumber<f32>()  );
    else if (Name == "alphaReference"   ) Materials_.Current->setAlphaReference     (readNumber<f32>()  );
    
    else if (Name == "colorMaterial"    ) Materials_.Current->setColorMaterial      (readBool()         );
    else if (Name == "lighting"         ) Materials_.Current->setLighting           (readBool()         );
    else if (Name == "blending"         ) Materials_.Current->setBlending           (readBool()         );
    else if (Name == "depthTest"        ) Materials_.Current->setDepthBuffer        (readBool()         );
    else if (Name == "fog"              ) Materials_.Current->setFog                (readBool()         );
    else if (Name == "polygonOffset"    ) Materials_.Current->setPolygonOffset      (readBool()         );
    
    else if (Name == "shading"          ) Materials_.Current->setShading            (PARSE_ENUM(parseShading    ));
    else if (Name == "wireframe"        ) Materials_.Current->setWireframe          (PARSE_ENUM(parseWireframe  ));
    else if (Name == "depthMethod"      ) Materials_.Current->setDepthMethod        (PARSE_ENUM(parseCompareType));
    else if (Name == "alphaMethod"      ) Materials_.Current->setAlphaMethod        (PARSE_ENUM(parseCompareType));
    else if (Name == "blendSource"      ) Materials_.Current->setBlendSource        (PARSE_ENUM(parseBlendType  ));
    else if (Name == "blendTarget"      ) Materials_.Current->setBlendTarget        (PARSE_ENUM(parseBlendType  ));
    else if (Name == "renderFace"       ) Materials_.Current->setRenderFace         (PARSE_ENUM(parseFaceType   ));
    
    else if (Name == "wireframeFront"   ) Materials_.Current->setWireframe          (PARSE_ENUM(parseWireframe), Materials_.Current->getWireframeBack());
    else if (Name == "wireframeBack"    ) Materials_.Current->setWireframe          (Materials_.Current->getWireframeFront(), PARSE_ENUM(parseWireframe));
    
    else
        breakUnexpectedIdentifier();
}

void MaterialScriptReader::readShaderClass()
{
    /* Read shader class name */
    nextTokenNoEOF();
    
    if (type() != TOKEN_STRING || Tkn_->Str.empty())
        breakExpectedIdentifier();
    
    const io::stringc& Name = Tkn_->Str;
    
    /* Check if shaders are supported */
    if (!GlbRenderSys->queryVideoSupport(video::VIDEOSUPPORT_SHADER))
    {
        io::Log::error("Can not create shader class \"" + Name + "\" because shaders are not supported by this render system");
        ignoreNextBlock();
        return;
    }
    
    /* Check if shader name already exists */
    if (findShader(Name) != 0)
        throw io::DefaultException("Multiple defintion of shader named \"" + Name + "\"");
    
    /* Read vertex input layout */
    nextTokenNoEOF();
    
    const video::VertexFormat* InputLayer = 0;
    
    if (type() == TOKEN_NAME)
    {
        InputLayer = parseVertexFormat(Tkn_->Str);
        if (!InputLayer)
            io::Log::warning("Unknown vertex format named \"" + Tkn_->Str + "\"");
    }
    else if (type() != TOKEN_BRACE_LEFT)
        breakUnexpectedToken();
    
    /* Create new shader */
    addShader(Name, InputLayer);
    
    /* Read shader class block */
    READ_SCRIPT_BLOCK(
        readShaderType();
    )
}

void MaterialScriptReader::readShaderType()
{
    const io::stringc& Name = Tkn_->Str;
    
    if (Name == "glsl" || Name == "glslEs" || Name == "hlsl3" || Name == "hlsl5")
    {
        if (validShaderForRenderSys(Name))
            readShader();
        else
            ignoreNextBlock();
    }
    else
        breakUnexpectedIdentifier();
}

void MaterialScriptReader::readShader()
{
    /* Read shader block */
    readBlockBegin();
    
    READ_SCRIPT_BLOCK(
        readAllShaderPrograms();
    )
    
    /* Compile shader class */
    Shaders_.Current->compile();
}

void MaterialScriptReader::readAllShaderPrograms()
{
    const video::EShaderTypes ShaderType = MaterialScriptReader::parseShaderType(Tkn_->Str);
    
    if (ShaderType != video::SHADER_DUMMY)
        readShaderProgram(ShaderType);
    else
        breakUnexpectedIdentifier();
}

void MaterialScriptReader::readShaderProgram(const video::EShaderTypes ShaderType)
{
    /* Read shader entry point or block begin */
    nextTokenNoEOF();
    
    if ( type() != TOKEN_BRACE_LEFT && ( type() != TOKEN_STRING || Tkn_->Str.empty() ) )
        throw io::DefaultException("Invalid shader entry point");
    
    io::stringc EntryPoint;
    
    if (type() != TOKEN_BRACE_LEFT)
    {
        EntryPoint = Tkn_->Str;
        
        /* Read block begin */
        nextTokenNoEOF();
        if (type() != TOKEN_BRACE_LEFT)
            breakUnexpectedToken();
    }
    else
    {
        /* Setup default entry point */
        switch (ShaderType)
        {
            case video::SHADER_VERTEX:      EntryPoint = "VertexMain";      break;
            case video::SHADER_PIXEL:       EntryPoint = "PixelMain";       break;
            case video::SHADER_GEOMETRY:    EntryPoint = "GeometryMain";    break;
            case video::SHADER_HULL:        EntryPoint = "HullMain";        break;
            case video::SHADER_DOMAIN:      EntryPoint = "DomainMain";      break;
            case video::SHADER_COMPUTE:     EntryPoint = "ComputeMain";     break;
            default:                                                        break;
        }
    }
    
    /* Read shader program block */
    READ_SCRIPT_BLOCK(
        readShaderProgramCode();
    )
    
    /* Create shader program */
    checkShaderVersion();
    
    if (!CurShaderBuffer_.empty())
    {
        GlbRenderSys->createShader(
            Shaders_.Current, ShaderType, CurShaderVersion_,
            CurShaderBuffer_, EntryPoint
        );
    }
    else
        io::Log::warning("Empty shader code");
    
    /* Reset internal state */
    CurShaderBuffer_.clear();
    CurShaderVersion_ = video::DUMMYSHADER_VERSION;
}

void MaterialScriptReader::readShaderProgramCode()
{
    const io::stringc& Name = Tkn_->Str;
    
    if (Name == "source")
    {
        checkShaderVersion();
        
        /* Read shader source code */
        CurShaderBuffer_.push_back(readString());
    }
    else if (Name == "sourceFile")
    {
        checkShaderVersion();
        
        /* Read shader source code from file */
        const io::stringc Filename = readString();
        
        io::FileSystem FileSys;
        video::ShaderClass::loadShaderResourceFile(
            FileSys, Filename, CurShaderBuffer_,
            CurShaderVersion_ == video::CG_VERSION_2_0
        );
    }
    else if (Name == "version")
        CurShaderVersion_ = PARSE_ENUM(parseShaderVersion);
    else
        breakUnexpectedIdentifier();
}

void MaterialScriptReader::readVertexFormat()
{
    /* Read vertex format name */
    nextTokenNoEOF();
    
    if (type() != TOKEN_STRING || Tkn_->Str.empty())
        breakExpectedIdentifier();
    
    const io::stringc& Name = Tkn_->Str;
    
    /* Check if vertex format name is reservered */
    if (Name.size() >= 12 && Name.leftEqual("vertexFormat"))
        throw io::DefaultException("Reserved vertex format name \"" + Name + "\" (May not begin with 'vertexFormat...')");
    
    /* Check if vertex format name already exists */
    if (findVertexFormat(Name) != 0)
        throw io::DefaultException("Multiple defintion of vertex format named \"" + Name + "\"");
    
    /* Create new vertex format */
    addVertexFormat(Name);
    
    /* Read vertex format block */
    readBlockBegin();
    
    READ_SCRIPT_BLOCK(
        readVertexFormatAttributes();
    )
}

void MaterialScriptReader::readVertexFormatAttributes()
{
    static const c8* VertAttribs[] =
    {
        "coord", "color", "normal", "binormal", "tangent", "texCoord", "fogCoord", "universal", 0
    };
    
    const io::stringc& Name = Tkn_->Str;
    
    /* Check if identifier is a valid vertex format attribute */
    const c8** Attrib = VertAttribs;
    
    while (*Attrib)
    {
        if (Name == *Attrib)
        {
            readVertexFormatAttributes(Name);
            return;
        }
        ++Attrib;
    }
    
    /* No valid attribute found -> unexpected identifier */
    breakUnexpectedIdentifier();
}

void MaterialScriptReader::readVertexFormatAttributes(const io::stringc &AttribType)
{
    /* Read attribute name */
    io::stringc AttribName;
    
    if (AttribType == "universal")
    {
        nextTokenNoEOF();
        
        if (type() != TOKEN_STRING || Tkn_->Str.empty())
            throw io::DefaultException("Universal without name is not allowed");
        
        AttribName = Tkn_->Str;
    }
    
    /* Attribute components */
    video::ERendererDataTypes DataType = video::DATATYPE_FLOAT;
    s32 Size = 3;
    bool Normalize = false;
    video::EVertexFormatFlags Attrib = video::VERTEXFORMAT_UNIVERSAL;
    
    /* Setup default configuration */
    if (AttribType == "color")
    {
        DataType = video::DATATYPE_UNSIGNED_BYTE;
        Size = 4;
    }
    else if (AttribType == "texCoord")
        Size = 2;
    
    /* Read vertex format attribute block */
    readBlockBegin();
    
    READ_SCRIPT_BLOCK(
        readVertexFormatAttributeComponents(DataType, Size, Normalize, Attrib);
    )
    
    /* Add final attribute */
         if (AttribType == "coord"      ) VertexFormats_.Current->addCoord      (DataType, Size );
    else if (AttribType == "color"      ) VertexFormats_.Current->addColor      (DataType, Size );
    else if (AttribType == "normal"     ) VertexFormats_.Current->addNormal     (DataType       );
    else if (AttribType == "binormal"   ) VertexFormats_.Current->addBinormal   (DataType       );
    else if (AttribType == "tangent"    ) VertexFormats_.Current->addTangent    (DataType       );
    else if (AttribType == "fogCoord"   ) VertexFormats_.Current->addFogCoord   (DataType       );
    else if (AttribType == "texCoord"   ) VertexFormats_.Current->addTexCoord   (DataType, Size );
    else
        VertexFormats_.Current->addUniversal(DataType, Size, AttribName, Normalize, Attrib);
}

void MaterialScriptReader::readVertexFormatAttributeComponents(
    video::ERendererDataTypes &DataType, s32 &Size, bool &Normalize, video::EVertexFormatFlags &Attrib)
{
    const io::stringc& Name = Tkn_->Str;
    
         if (Name == "size"     ) Size      = readNumber<s32>();
    else if (Name == "type"     ) DataType  = PARSE_ENUM(parseDataType);
    else if (Name == "normalize") Normalize = readBool();
    else if (Name == "attribute") Attrib    = PARSE_ENUM(parseFormatFlag);
    else
        breakUnexpectedIdentifier();
}

void MaterialScriptReader::readTexture()
{
    /* Read texture name */
    nextTokenNoEOF();
    
    if (type() != TOKEN_STRING || Tkn_->Str.empty())
        breakExpectedIdentifier();
    
    const io::stringc& Name = Tkn_->Str;
    
    /* Check if texture name already exists */
    if (findTexture(Name) != 0)
        throw io::DefaultException("Multiple defintion of texture named \"" + Name + "\"");
    
    /* Read texture block */
    readBlockBegin();
    
    READ_SCRIPT_BLOCK(
        readTextureAttributes();
    )
    
    /* Create final texture */
    video::Texture* Tex = 0;
    
    if (!CurTexFlags_.Filename.empty())
    {
        /* Load texture from file */
        Tex = GlbRenderSys->loadTexture(CurTexFlags_.Filename);
        
        /* Setup texture creation flags subsequently */
        Tex->setFilter(CurTexFlags_.Filter);
        Tex->setFormat(CurTexFlags_.Format);
        Tex->setHardwareFormat(CurTexFlags_.HWFormat);
        
        if (CurTexFlags_.Size.Width > 0 && CurTexFlags_.Size.Height > 0)
            Tex->setSize(CurTexFlags_.Size);
        if (CurTexFlags_.Type != video::TEXTURE_2D)
            Tex->setType(CurTexFlags_.Type, CurTexFlags_.Depth);
    }
    else
    {
        /* Create custom texture */
        Tex = GlbRenderSys->createTexture(CurTexFlags_);
        
        /* Setup fill color */
        if (!CurTexRenderTarget_)
            fillImageBuffer(Tex, CurFillColor_);
    }
    
    addTexture(Name, Tex);
    
    /* Setup additional configuration */
    if (CurTexRenderTarget_)
        Tex->setRenderTarget(true);
    if (CurColorKey_.Alpha < 255)
        Tex->setColorKey(CurColorKey_);
    
    /* Reset internal state */
    CurColorKey_        = video::color();
    CurFillColor_       = video::color();
    CurTexRenderTarget_ = false;
}

void MaterialScriptReader::readTextureAttributes()
{
    const io::stringc& Name = Tkn_->Str;
    
         if (Name == "imageFile"    ) CurTexFlags_.Filename     = readString();
    else if (Name == "fillColor"    ) CurFillColor_             = readColor();
    else if (Name == "type"         ) CurTexFlags_.Type         = PARSE_ENUM(parseTextureType   );
    else if (Name == "bufferType"   ) CurTexFlags_.BufferType   = PARSE_ENUM(parseBufferType    );
    else if (Name == "format"       ) CurTexFlags_.Format       = PARSE_ENUM(parsePixelFormat   );
    else if (Name == "formatHW"     ) CurTexFlags_.HWFormat     = PARSE_ENUM(parseHWTexFormat   );
    else if (Name == "width"        ) CurTexFlags_.Size.Width   = readNumber<s32>();
    else if (Name == "height"       ) CurTexFlags_.Size.Height  = readNumber<s32>();
    else if (Name == "depth"        ) CurTexFlags_.Depth        = readNumber<s32>();
    else if (Name == "colorKey"     ) CurColorKey_              = readColor();
    else if (Name == "renderTarget" ) CurTexRenderTarget_       = readBool();
    else if (Name == "filter"       ) readTextureFilter();
    else
        breakUnexpectedIdentifier();
}

void MaterialScriptReader::readTextureFilter()
{
    /* Read texture filter block */
    readBlockBegin();
    
    READ_SCRIPT_BLOCK(
        readTextureFilterAttributes();
    )
}

void MaterialScriptReader::readTextureFilterAttributes()
{
    const io::stringc& Name = Tkn_->Str;
    video::STextureFilter& Filter = CurTexFlags_.Filter;
    
         if (Name == "mipMaps"      ) Filter.HasMIPMaps = readBool();
    else if (Name == "anisotropy"   ) Filter.Anisotropy = readNumber<s32>();
    else if (Name == "wrap"         ) Filter.WrapMode   = PARSE_ENUM(parseTexWrapMode   );
    else if (Name == "wrapX"        ) Filter.WrapMode.X = PARSE_ENUM(parseTexWrapMode   );
    else if (Name == "wrapY"        ) Filter.WrapMode.Y = PARSE_ENUM(parseTexWrapMode   );
    else if (Name == "wrapZ"        ) Filter.WrapMode.Z = PARSE_ENUM(parseTexWrapMode   );
    else if (Name == "min"          ) Filter.Min        = PARSE_ENUM(parseTexFilter     );
    else if (Name == "mag"          ) Filter.Mag        = PARSE_ENUM(parseTexFilter     );
    else if (Name == "mip"          ) Filter.MIPMap     = PARSE_ENUM(parseMIPMapFilter  );
    else
        breakUnexpectedIdentifier();
}

void MaterialScriptReader::readTextureLayer()
{
    /* Read texture layer name */
    nextTokenNoEOF();
    
    if (type() != TOKEN_STRING || Tkn_->Str.empty())
        breakExpectedIdentifier();
    
    const io::stringc& Name = Tkn_->Str;
    
    /* Check if texture layer name already exists */
    if (findTextureLayer(Name) != 0)
        throw io::DefaultException("Multiple defintion of texture layer named \"" + Name + "\"");
    
    /* Read texture layer type */
    nextTokenNoEOF();
    
    if (type() != TOKEN_NAME)
        breakUnexpectedToken();
    
    const io::stringc& LayerType = Tkn_->Str;
    
    /* Create new texture layer */
    addTextureLayer(Name, LayerType);
    
    /* Read texture layer block */
    readBlockBegin();
    
    READ_SCRIPT_BLOCK(
        readTextureLayerAttributes();
    )
}

void MaterialScriptReader::readTextureLayerAttributes()
{
    #define SET_TEXLAYER_STD(f)                                                                 \
        {                                                                                       \
            video::TextureLayerStandardPtr TexLayerStd =                                        \
                boost::dynamic_pointer_cast<video::TextureLayerStandard>(TexLayers_.Current);   \
            if (TexLayerStd)                                                                    \
                TexLayerStd->f;                                                                 \
        }
    #define SET_TEXLAYER_RLF(f)                                                             \
        {                                                                                   \
            video::TextureLayerReliefPtr TexLayerRlf =                                      \
                boost::dynamic_pointer_cast<video::TextureLayerRelief>(TexLayers_.Current); \
            if (TexLayerRlf)                                                                \
                TexLayerRlf->f;                                                             \
        }
    
    const io::stringc& Name = Tkn_->Str;
    
    /* Setup base settings */
         if (Name == "tex"              ) TexLayers_.Current->setTexture    (findTexture(readIdentifier())  );
    else if (Name == "enable"           ) TexLayers_.Current->setEnable     (readBool()                     );
    else if (Name == "visibleMask"      ) TexLayers_.Current->setVisibleMask(readNumber<s32>()              );
    else if (Name == "index"            ) TexLayers_.Current->setIndex      (readNumber<u8>()               );
    /* Setup standard settings */
    else if (Name == "environment"      ) SET_TEXLAYER_STD(setTextureEnv(PARSE_ENUM(parseTextureEnv)))
    else if (Name == "mapping"          ) SET_TEXLAYER_STD(setMappingGen(PARSE_ENUM(parseMappingGen)))
    /* Setup relief settings */
    else if (Name == "reliefEnable"     ) SET_TEXLAYER_RLF(setReliefEnable  (readBool()         ))
    else if (Name == "heightMapScale"   ) SET_TEXLAYER_RLF(setHeightMapScale(readNumber<f32>()  ))
    else if (Name == "viewRange"        ) SET_TEXLAYER_RLF(setViewRange     (readNumber<f32>()  ))
    else if (Name == "minSamples"       ) SET_TEXLAYER_RLF(setMinSamples    (readNumber<s32>()  ))
    else if (Name == "maxSamples"       ) SET_TEXLAYER_RLF(setMaxSamples    (readNumber<s32>()  ))
    else
        breakUnexpectedIdentifier();
    
    #undef SET_TEXLAYER_STD
    #undef SET_TEXLAYER_RLF
}

void MaterialScriptReader::readVarDefinition()
{
    /* Check if a variable is about to be defined */
    if (type() != TOKEN_AT)
        return;
    
    enableNL();
    
    /* Read variable name */
    const io::stringc Name = readVarName();
    
    /* Check if variable is already registered */
    if (hasVariable(Name))
        io::Log::warning("Multiple definition of variable named \"" + Name + "\"");
    
    /* Check if the name is followed by an assignment character */
    nextTokenNoEOF();
    
    if (type() != TOKEN_EQUAL)
        breakExpectedAssignment();
    
    /* Read variable initialization */
    io::stringc StrVal;
    f64 NumVal = 0.0;
    
    bool HasAnyVal = false;
    bool IsVarStr = false;
    bool IsNumNegative = false;
    
    while (1)
    {
        /* Read next token */
        nextToken();
        
        if (type() == TOKEN_NEWLINE)
            break;
        
        /* Check if strings will be added */
        if (HasAnyVal)
        {
            /* Check if initialization has started as string */
            if (!IsVarStr)
                breakSingleNumberOnly();
            
            /* Check if the previous string is followed by a '+' character */
            if (type() != TOKEN_ADD)
                breakStringCombination();
            
            /* Read next token after '+' character */
            nextTokenNoEOF();
            if (type() == TOKEN_NEWLINE)
                throw io::DefaultException("No more expressions after '+' character");
        }
        /* Check if token is a negative number */
        else if (type() == TOKEN_SUB)
        {
            /* Setup variable as negative number  */
            IsNumNegative = true;
            
            /* Read next token after '-' character */
            nextTokenNoEOF();
            if (type() == TOKEN_NEWLINE)
                throw io::DefaultException("No more expressions after '-' character");
        }
        
        /* Check if token is a number */
        if (type() == TOKEN_NUMBER_INT || type() == TOKEN_NUMBER_FLOAT)
        {
            /* Setup variable as number */
            NumVal = Tkn_->Str.val<f64>();
            
            if (IsNumNegative)
                NumVal = -NumVal;
        }
        /* Check if token is a variable */
        else if (type() == TOKEN_AT)
        {
            /* Read variable name */
            const io::stringc SubVarName = readVarName();
            
            /* Get variable value */
            io::stringc SubStrVal;
            f64 SubNumVal = 0.0;
            bool IsSubVarStr = false;
            
            getVarValue(SubVarName, SubStrVal, SubNumVal, IsSubVarStr);
            
            /* Add variable value */
            if (IsSubVarStr)
            {
                if (IsNumNegative)
                    throw io::DefaultException("Strings can not be negative");
                
                StrVal += SubStrVal;
                IsVarStr = true;
            }
            else
            {
                NumVal = SubNumVal;
                
                if (IsNumNegative)
                    NumVal = -NumVal;
            }
        }
        else if (type() == TOKEN_STRING)
        {
            /* Add variable value */
            IsVarStr = true;
            StrVal += Tkn_->Str;
        }
        else
            breakUnexpectedToken();
        
        HasAnyVal = true;
    }
    
    /* Check if initialization is empty */
    if (!HasAnyVal)
        throw io::DefaultException("Variable definition without initialization");
    
    /* Register new variable */
    if (IsVarStr)
        registerString(Name, StrVal);
    else
        registerNumber(Name, NumVal);
    
    disableNL();
}

void MaterialScriptReader::readAssignment()
{
    /* Read assignement character */
    nextTokenNoEOF();
    
    if (type() != TOKEN_EQUAL)
        breakUnexpectedToken();
    
    /* Read next token to continue parsing */
    nextTokenNoEOF();
}

void MaterialScriptReader::readBlockBegin()
{
    /* Read block begin character '{' */
    nextTokenNoEOF();
    
    if (type() != TOKEN_BRACE_LEFT)
        breakUnexpectedToken();
}

io::stringc MaterialScriptReader::readVarName()
{
    /* Read variable name */
    nextTokenNoEOF(false);
    
    if (type() != TOKEN_NAME)
        breakExpectedIdentifier();
    
    return Tkn_->Str;
}

f64 MaterialScriptReader::readDouble(bool ReadAssignment)
{
    /* Read assignment character */
    if (ReadAssignment)
        readAssignment();
    
    /* Check if the number is negative */
    f64 Factor = 1.0;
    
    if (type() == TOKEN_SUB)
    {
        Factor = -1.0;
        nextTokenNoEOF();
    }
    
    /* Read float number */
    switch (type())
    {
        case TOKEN_NUMBER_INT:
        case TOKEN_NUMBER_FLOAT:
            return Factor * Tkn_->Str.val<f64>();
            
        case TOKEN_AT:
            /* Read variable name */
            nextTokenNoEOF();
            
            if (type() != TOKEN_NAME)
                breakExpectedIdentifier();
            
            /* Return variable name */
            return Factor * getNumber(Tkn_->Str);
            
        default:
            breakUnexpectedToken();
    }
    
    return 0.0;
}

io::stringc MaterialScriptReader::readString(bool ReadAssignment)
{
    enableNL();
    
    /* Read assignment character */
    if (ReadAssignment)
        readAssignment();
    
    if (type() == TOKEN_NEWLINE)
        breakExpectedString();
    
    io::stringc Str;
    
    while (1)
    {
        /* Add string value */
        if (type() == TOKEN_STRING)
            Str += Tkn_->Str;
        else if (type() == TOKEN_AT)
            Str += getString(readVarName());
        else
            throw io::DefaultException("Excepted string or string-variable");
        
        /* Read next token (new-line or '+' character) */
        nextToken();
        
        if (type() == TOKEN_NEWLINE)
            break;
        else if (type() != TOKEN_ADD)
            breakStringCombination();
        
        /* Read next token (must be a string or a variable) */
        nextTokenNoEOF();
    }
    
    disableNL();
    
    return Str;
}

io::stringc MaterialScriptReader::readIdentifier(bool ReadAssignment)
{
    /* Read assignment character */
    if (ReadAssignment)
        readAssignment();
    
    /* Read identifier name */
    if (type() != TOKEN_NAME)
        breakUnexpectedToken();
    
    return Tkn_->Str;
}

bool MaterialScriptReader::readBool(bool ReadAssignment)
{
    /* Read boolean keyword */
    const io::stringc Keyword = readIdentifier(ReadAssignment);
    
    if (Keyword == "true")
        return true;
    else if (Keyword == "false")
        return false;
    
    /* Unknwon keyword -> throw exception */
    throw io::DefaultException("Unknown keyword \"" + Keyword + "\"");
    
    return false;
}

video::color MaterialScriptReader::readColor(bool ReadAssignment)
{
    enableNL();
    
    /* Read assignment character */
    if (ReadAssignment)
        readAssignment();
    
    /* Read color components */
    video::color Color;
    u32 Comp = 0;
    
    while (type() != TOKEN_NEWLINE)
    {
        /* Read color component */
        if (Comp < 4)
            Color[Comp++] = readNumber<u8>(false);
        
        /* Read comma or new-line character */
        nextTokenNoEOF();
        
        if (type() == TOKEN_NEWLINE)
            break;
        else if (type() != TOKEN_COMMA)
            breakUnexpectedToken();
        
        nextTokenNoEOF();
    }
    
    if (Comp == 1)
        Color = video::color(Color[0]);
    
    disableNL();
    
    return Color;
}

void MaterialScriptReader::clearVariables()
{
    StringVariables_.clear();
    NumericVariables_.clear();
}

void MaterialScriptReader::checkShaderVersion()
{
    if (CurShaderVersion_ == video::DUMMYSHADER_VERSION)
        throw io::DefaultException("No shader version specified");
}

void MaterialScriptReader::fillImageBuffer(video::Texture* Tex, const video::color &FillColor) const
{
    if (!Tex)
        return;
    
    video::ImageBuffer* ImgBuffer = Tex->getImageBuffer();
    
    switch (Tex->getImageBuffer()->getType())
    {
        case video::IMAGEBUFFER_UBYTE:
        {
            for (s32 y = 0; y < Tex->getSize().Height; ++y)
            {
                for (s32 x = 0; x < Tex->getSize().Width; ++x)
                    ImgBuffer->setPixelColor(dim::point2di(x, y), FillColor);
            }
        }
        break;
        
        case video::IMAGEBUFFER_FLOAT:
        {
            /* Get fill color as vector */
            dim::vector4df VecColor;
            FillColor.getFloatArray(&VecColor[0]);
            
            for (s32 y = 0; y < Tex->getSize().Height; ++y)
            {
                for (s32 x = 0; x < Tex->getSize().Width; ++x)
                    ImgBuffer->setPixelVector(dim::point2di(x, y), VecColor);
            }
        }
        break;
        
        default:
            break;
    }
    
    Tex->updateImageBuffer();
}

bool MaterialScriptReader::validShaderForRenderSys(const io::stringc &Name) const
{
    switch (GlbRenderSys->getRendererType())
    {
        case video::RENDERER_OPENGL:        return Name == "glsl";
        case video::RENDERER_OPENGLES2:     return Name == "glslEs";
        case video::RENDERER_DIRECT3D9:     return Name == "hlsl3";
        case video::RENDERER_DIRECT3D11:    return Name == "hlsl5";
        default:                            break;
    }
    return false;
}

bool MaterialScriptReader::readScriptBlock()
{
    const io::stringc& Name = Tkn_->Str;
    
         if (Name == "material"     ) readMaterial      ();
    else if (Name == "shader"       ) readShaderClass   ();
    else if (Name == "vertexFormat" ) readVertexFormat  ();
    else if (Name == "texture"      ) readTexture       ();
    else if (Name == "textureLayer" ) readTextureLayer  ();
    else
        return false;
    
    return true;
}

void MaterialScriptReader::defineDefaultVariables()
{
    registerString("workingDir", GlbEngineDev->getWorkingDir());
}

#undef READ_SCRIPT_BLOCK
#undef PARSE_ENUM


} // /namespace tool

} // /namespace sp


#endif



// ================================================================================
