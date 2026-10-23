#include "ContactListener.hpp"

#include "Game/Scene.hpp"
#include "Game/Player.hpp"
#include "Physics.hpp"
#include "Foundation/Memory.hpp"

#include <Jolt/Physics/Body/Body.h>

VoidContactListener::VoidContactListener()
{
    toDeleteQueue.init(&MemoryService::instance()->systemAllocator, 16);
}

// See: ContactListener
JPH::ValidateResult	VoidContactListener::OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult)
{
    //vprint("Contact validate validate.\n");
    uintptr_t pointer1 = inBody1.GetUserData();
    if (pointer1 != 0)
    {
        Entity* currentEntity = ((Entity*)pointer1);
        switch (currentEntity->entityType)
        {
        case EntityType::PLAYER:
            static_cast<Player*>(currentEntity->entityData)->crashNoise();
            break;
        case EntityType::ROCK:
            if (currentEntity->isDeleted == false)
            {
                toDeleteQueue.push(currentEntity->entityIndex);
            }
            break;
	default:
	    break;
        }
    }

    uintptr_t pointer2 = inBody2.GetUserData();
    if (pointer2 != 0)
    {
        Entity* currentEntity = ((Entity*)pointer2);
        switch (currentEntity->entityType)
        {
        case EntityType::PLAYER:
            static_cast<Player*>(currentEntity->entityData)->crashNoise();
            break;
        case EntityType::ROCK:
            if (currentEntity->isDeleted == false)
            {
                toDeleteQueue.push(currentEntity->entityIndex);
            }
            break;
        default:
	    break;
        }
    }

    // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void VoidContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
    //vprint("Contact validate added.\n");
}

void VoidContactListener::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
    //vprint("Contact validate persisted.\n");
}

void VoidContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
{
    //vprint("Contact validate removed.\n");
}
