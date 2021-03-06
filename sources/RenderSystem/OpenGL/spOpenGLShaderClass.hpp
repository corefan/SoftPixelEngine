/*
 * OpenGL shader class header
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

#ifndef __SP_OPENGL_SHADERCLASS_H__
#define __SP_OPENGL_SHADERCLASS_H__


#include "Base/spStandard.hpp"

#if defined(SP_COMPILE_WITH_OPENGL) || defined(SP_COMPILE_WITH_OPENGLES2)


#include "RenderSystem/spShaderProgram.hpp"
#include "RenderSystem/OpenGL/spOpenGLCoreHeader.hpp"
#include "RenderSystem/OpenGLES/spOpenGLESFunctionsARB.hpp"


namespace sp
{
namespace video
{


class OpenGLShader;

class SP_EXPORT OpenGLShaderClass : public ShaderClass
{
    
    public:
        
        OpenGLShaderClass(const VertexFormat* VertexInputLayout = 0);
        ~OpenGLShaderClass();
        
        /* === Functions === */
        
        void bind(const scene::MaterialNode* Object = 0);
        void unbind();
        
        bool compile();
        
    private:
        
        friend class OpenGLShader;
        friend class OpenGLConstantBuffer;
        friend class GLProgrammableFunctionPipeline;
        
        /* === Functions === */
        
        void deleteShaderObject(Shader* ShaderObject);
        bool checkLinkingErrors();

        bool setupUniforms();
        bool setupUniformBlocks();
        
        void setupVertexFormat(const VertexFormat* VertexInputLayout);
        
        void addShaderConstant(const c8* Name, const GLenum Type, u32 Count, s32 Location);
        
        /* === Members === */
        
        GLuint ProgramObject_;
        
        const VertexFormat* VertexInputLayout_;
        
        static GLuint LastProgramObject_;
        
};


} // /namespace scene

} // /namespace sp


#endif

#endif



// ================================================================================
