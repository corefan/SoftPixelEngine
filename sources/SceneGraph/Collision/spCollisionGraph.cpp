/*
 * Collision graph file
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

#include "SceneGraph/Collision/spCollisionGraph.hpp"
#include "Base/spMemoryManagement.hpp"

#include <boost/foreach.hpp>


namespace sp
{
namespace scene
{


static bool cmpIntersectionContacts(SIntersectionContact &ContactA, SIntersectionContact &ContactB)
{
    return ContactA.DistanceSq < ContactB.DistanceSq;
}

CollisionGraph::CollisionGraph() :
    RootTreeNode_(0)
{
}
CollisionGraph::~CollisionGraph()
{
    clearScene();
}

CollisionMaterial* CollisionGraph::createMaterial()
{
    CollisionMaterial* NewMaterial = MemoryManager::createMemory<CollisionMaterial>("CollisionMaterial");
    CollMaterials_.push_back(NewMaterial);
    return NewMaterial;
}
void CollisionGraph::deleteMaterial(CollisionMaterial* Material)
{
    MemoryManager::removeElement(CollMaterials_, Material, true);
}

void CollisionGraph::addCollisionNode(CollisionNode* Node)
{
    if (Node)
        CollNodes_.push_back(Node);
}
void CollisionGraph::removeCollisionNode(CollisionNode* Node)
{
    MemoryManager::removeElement(CollNodes_, Node);
}

CollisionSphere* CollisionGraph::createSphere(CollisionMaterial* Material, scene::SceneNode* Node, f32 Radius)
{
    try
    {
        return addCollNode(new CollisionSphere(Material, Node, Radius));
    }
    catch (const io::stringc &ErrorStr)
    {
        io::Log::error(ErrorStr);
    }
    return 0;
}

CollisionCapsule* CollisionGraph::createCapsule(CollisionMaterial* Material, scene::SceneNode* Node, f32 Radius, f32 Height)
{
    try
    {
        return addCollNode(new CollisionCapsule(Material, Node, Radius, Height));
    }
    catch (const io::stringc &ErrorStr)
    {
        io::Log::error(ErrorStr);
    }
    return 0;
}

CollisionCylinder* CollisionGraph::createCylinder(CollisionMaterial* Material, scene::SceneNode* Node, f32 Radius, f32 Height)
{
    try
    {
        return addCollNode(new CollisionCylinder(Material, Node, Radius, Height));
    }
    catch (const io::stringc &ErrorStr)
    {
        io::Log::error(ErrorStr);
    }
    return 0;
}

CollisionCone* CollisionGraph::createCone(CollisionMaterial* Material, scene::SceneNode* Node, f32 Radius, f32 Height)
{
    try
    {
        return addCollNode(new CollisionCone(Material, Node, Radius, Height));
    }
    catch (const io::stringc &ErrorStr)
    {
        io::Log::error(ErrorStr);
    }
    return 0;
}

CollisionBox* CollisionGraph::createBox(CollisionMaterial* Material, scene::SceneNode* Node, const dim::aabbox3df &Box)
{
    try
    {
        return addCollNode(new CollisionBox(Material, Node, Box));
    }
    catch (const io::stringc &ErrorStr)
    {
        io::Log::error(ErrorStr);
    }
    return 0;
}

CollisionPlane* CollisionGraph::createPlane(CollisionMaterial* Material, scene::SceneNode* Node, const dim::plane3df &Plane)
{
    try
    {
        return addCollNode(new CollisionPlane(Material, Node, Plane));
    }
    catch (const io::stringc &ErrorStr)
    {
        io::Log::error(ErrorStr);
    }
    return 0;
}

CollisionMesh* CollisionGraph::createMesh(CollisionMaterial* Material, scene::Mesh* Mesh, u8 MaxTreeLevel)
{
    try
    {
        return addCollNode(new CollisionMesh(Material, Mesh, MaxTreeLevel));
    }
    catch (const io::stringc &ErrorStr)
    {
        io::Log::error(ErrorStr);
    }
    return 0;
}

CollisionMesh* CollisionGraph::createMeshList(CollisionMaterial* Material, const std::list<Mesh*> &MeshList, u8 MaxTreeLevel)
{
    try
    {
        return addCollNode(new CollisionMesh(Material, MeshList, MaxTreeLevel));
    }
    catch (const io::stringc &ErrorStr)
    {
        io::Log::error(ErrorStr);
    }
    return 0;
}

bool CollisionGraph::deleteNode(CollisionNode* Node)
{
    return MemoryManager::removeElement(CollNodes_, Node, true);
}

CharacterController* CollisionGraph::createCharacterController(
    CollisionMaterial* Material, scene::SceneNode* Node, f32 Radius, f32 Height)
{
    try
    {
        CharacterController* NewObject = new CharacterController(Material, Node, Radius, Height);
        CharacterControllers_.push_back(NewObject);
        return NewObject;
    }
    catch (const io::stringc &ErrorStr)
    {
        io::Log::error(ErrorStr);
    }
    return 0;
}
bool CollisionGraph::deleteCharacterController(CharacterController* Object)
{
    return MemoryManager::removeElement(CharacterControllers_, Object, true);
}

void CollisionGraph::clearScene(
    bool isDeleteNodes, bool isDeleteMaterials, bool isDeleteCharacters)
{
    if (isDeleteNodes)
        MemoryManager::deleteList(CollNodes_);
    if (isDeleteMaterials)
        MemoryManager::deleteList(CollMaterials_);
    if (isDeleteCharacters)
        MemoryManager::deleteList(CharacterControllers_);
}

bool CollisionGraph::checkIntersection(
    const dim::line3df &Line, bool ExcludeCorners, const IntersectionCriteriaCallback &CriteriaCallback) const
{
    if (RootTreeNode_)
    {
        // !todo!
        #if 0
        /* Find tree leaf nodes */
        std::list<const TreeNode*> TreeNodeList;
        RootTreeNode_->findLeafList(TreeNodeList, Line);
        
        foreach (const TreeNode* Node, TreeNodeList)
        {
            //...
        }
        #endif
    }
    else
    {
        /* Check all collision nodes for intersection */
        foreach (CollisionNode* Node, CollNodes_)
        {
            if ( ( !CriteriaCallback || CriteriaCallback(Node) ) &&
                 (Node->getFlags() & COLLISIONFLAG_INTERSECTION) && Node->checkIntersection(Line, ExcludeCorners) )
            {
                return true;
            }
        }
    }
    
    return false;
}

void CollisionGraph::findIntersections(
    const dim::line3df &Line, std::list<SIntersectionContact> &ContactList,
    bool SearchBidirectional, const IntersectionCriteriaCallback &CriteriaCallback) const
{
    findIntersectionsUnidirectional(Line, ContactList, CriteriaCallback);
    
    if (SearchBidirectional)
        findIntersectionsUnidirectional(Line.getViceVersa(), ContactList, CriteriaCallback);
    
    CollisionGraph::sortContactList(Line.Start, ContactList);
}

void CollisionGraph::updateScene()
{
    if (RootTreeNode_)
    {
        // !todo!
        #if 0
        /* Find tree leaf nodes */
        std::list<const TreeNode*> TreeNodeList;
        RootTreeNode_->findLeafList(TreeNodeList, Line);
        
        foreach (const TreeNode* Node, TreeNodeList)
        {
            //...
        }
        #endif
    }
    else
    {
        //!TODO! -> optimize for-loop againts none-resolving objects
        //!TODO! -> distinguish between static and dynamic objects
        
        /* Check all collision nodes for resolving */
        foreach (CollisionNode* Node, CollNodes_)
            Node->updateCollisions();
    }
}

void CollisionGraph::sortContactList(const dim::vector3df &LineStart, std::list<SIntersectionContact> &ContactList)
{
    /* Store squared distance for each contact */
    foreach (SIntersectionContact &Contact, ContactList)
        Contact.DistanceSq = math::getDistanceSq(LineStart, Contact.Point);
    
    /* Make resulting list unique and sorted */
    //ContactList.unique();
    ContactList.sort(cmpIntersectionContacts);
}


/*
 * ======= Protected: =======
 */

void CollisionGraph::findIntersectionsUnidirectional(
    const dim::line3df &Line, std::list<SIntersectionContact> &ContactList,
    const IntersectionCriteriaCallback &CriteriaCallback) const
{
    if (RootTreeNode_)
    {
        // !todo!
        #if 0
        /* Find tree leaf nodes */
        std::list<const TreeNode*> TreeNodeList;
        RootTreeNode_->findLeafList(TreeNodeList, Line);
        
        foreach (const TreeNode* Node, TreeNodeList)
        {
            //...
        }
        #endif
    }
    else
    {
        /* Check all collision nodes for intersection */
        //todo -> foreach (CollisionNode* Node, CollNodes_) { add Node to ContactList data ... }
        foreach (const CollisionNode* Node, CollNodes_)
        {
            if ( ( !CriteriaCallback || CriteriaCallback(Node) ) && (Node->getFlags() & COLLISIONFLAG_INTERSECTION) )
                Node->findIntersections(Line, ContactList);
        }
    }
}


} // /namespace scene

} // /namespace sp



// ================================================================================
