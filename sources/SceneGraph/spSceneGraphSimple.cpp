/*
 * Simple scene graph file
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

#include "SceneGraph/spSceneGraphSimple.hpp"

#ifdef SP_COMPILE_WITH_SCENEGRAPH_SIMPLE


#include "RenderSystem/spRenderSystem.hpp"

#include <boost/foreach.hpp>


namespace sp
{

extern video::RenderSystem* __spVideoDriver;

namespace scene
{


SceneGraphSimple::SceneGraphSimple() :
    SceneGraph(SCENEGRAPH_SIMPLE)
{
    /* Setup plain material */
    MaterialPlain_.setLighting      (false);
    MaterialPlain_.setBlending      (false);
    MaterialPlain_.setFog           (false);
    MaterialPlain_.setColorMaterial (false);
}
SceneGraphSimple::~SceneGraphSimple()
{
}

void SceneGraphSimple::render()
{
    __spVideoDriver->setRenderMode(video::RENDERMODE_SCENE);
    
    /* Update scene graph transformation */
    const dim::matrix4f BaseMatrix(getTransformMatrix(true));
    
    /* Render lights */
    renderLightsDefault(BaseMatrix);
    
    /* Render geometry */
    arrangeRenderList(RenderList_, BaseMatrix);
    
    if (DepthSorting_)
    {
        foreach (RenderNode* Node, RenderList_)
        {
            if (!Node->getVisible())
                break;
            Node->render();
        }
    }
    else
    {
        foreach (RenderNode* Node, RenderList_)
        {
            if (Node->getVisible())
                Node->render();
        }
    }
    
    __spVideoDriver->setRenderMode(video::RENDERMODE_NONE);
}

void SceneGraphSimple::renderScenePlain(Camera* ActiveCamera)
{
    if (!ActiveCamera)
        return;
    
    /* Begin with scene rendering */
    __spVideoDriver->setRenderMode(video::RENDERMODE_SCENE);
    
    /* Setup active camera for drawing the scene */
    setActiveCamera(ActiveCamera);
    ActiveCamera->setupRenderView();
    
    /* Update scene graph transformation */
    spWorldMatrix.reset();
    
    const dim::matrix4f BaseMatrix(getTransformMatrix(true));
    
    /* Setup default material states */
    __spVideoDriver->setupMaterialStates(&MaterialPlain_);
    
    /* Render geometry */
    foreach (RenderNode* Node, RenderList_)
    {
        if (Node->getType() != scene::NODE_MESH || !Node->getVisible())
            continue;
        
        /* Render mesh object plain */
        Mesh* MeshObj = static_cast<scene::Mesh*>(Node);
        
        /* Matrix transformation */
        MeshObj->updateTransformation();
        MeshObj->loadTransformation();
        
        /* Frustum culling */
        if (!MeshObj->getBoundingVolume().checkFrustumCulling(ActiveCamera->getViewFrustum(), __spVideoDriver->getWorldMatrix()))
            return;
        
        /* Update the render matrix */
        __spVideoDriver->updateModelviewMatrix();
        
        /* Setup shader class */
        __spVideoDriver->setupShaderClass(MeshObj, MeshObj->getShaderClass());
        
        /* Draw the current mesh object */
        foreach (video::MeshBuffer* Surface, MeshObj->getMeshBufferList())
            __spVideoDriver->drawMeshBufferPlain(Surface, true);
    }
    
    /* Finish rendering the scene */
    __spVideoDriver->setRenderMode(video::RENDERMODE_NONE);
}


} // /namespace scene

} // /namespace sp


#endif



// ================================================================================
