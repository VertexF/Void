#ifndef PHYSICS_HDR
#define PHYSICS_HDR

#include "Foundation/Platform.hpp"
#include "Foundation/Log.hpp"
#include "Foundation/Memory.hpp"

#include "ContactListener.hpp"

#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>


// Disable common warnings triggered by Jolt, you can use JPH_SUPPRESS_WARNING_PUSH / JPH_SUPPRESS_WARNING_POP to store and restore the warning state
JPH_SUPPRESS_WARNINGS

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace Layers
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer CHARACTER = 2;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 3;
};

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    virtual ~ObjectLayerPairFilterImpl() = default;

    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
        case Layers::NON_MOVING:
            return false; // Non moving only collides with moving
        case Layers::MOVING:
            return false; // Moving collides with everything
        case Layers::CHARACTER:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::BroadPhaseLayer CHARACTER(2);//character
    static constexpr uint32_t NUM_LAYERS(3);
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BroadPhaseLayerImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    virtual ~BroadPhaseLayerImpl() = default;

    BroadPhaseLayerImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::CHARACTER] = BroadPhaseLayers::CHARACTER;
    }

    virtual uint32_t GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        switch ((BroadPhaseLayer::Type)inLayer)
        {
        case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
        case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
        default:													JPH_ASSERT(false); return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    virtual ~ObjectVsBroadPhaseLayerFilterImpl() = default;

    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        case Layers::CHARACTER:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

// An example activation listener
class MyBodyActivationListener : public JPH::BodyActivationListener
{
public:
    virtual ~MyBodyActivationListener() = default;

    virtual void OnBodyActivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override
    {
        //vprint("A body got activated.\n");
    }

    virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override
    {
        //vprint("A body went to sleep.\n");
    }
};

//This is here because we need to have the temporary allocator for the physics.
//When we are working with jolt we need to use inhertance we can't get around this. We have used a stack allocator instead of a heap based on.
class JPH_EXPORT PhysicAllocator final : public JPH::TempAllocator
{
public:
    JPH_OVERRIDE_NEW_DELETE

    virtual	~PhysicAllocator() = default;

    explicit PhysicAllocator()
    {
        allocator = &MemoryService::instance()->physicsAllocator;
    }

    virtual void* Allocate(unsigned int inSize) override
    {
        //Jolt can allocate "0" piece of memory and this is valid.
        if (inSize == 0)
        {
            return nullptr;
        }

        return allocator->allocate(inSize * JPH_RVECTOR_ALIGNMENT, JPH_RVECTOR_ALIGNMENT);
    }

    virtual void Free(void* inAddress, unsigned int inSize) override
    {
        if (inAddress == nullptr)
        {
            JPH_ASSERT(inSize == 0);
        }
        else
        {
            allocator->freeMarker(inSize);
        }
    }

    //Check if no allocations have been made
    bool IsEmpty() const
    {
        return allocator->allocatedSize == 0;
    }

    //Get the total size of the fixed buffer
    size_t GetSize() const
    {
        return allocator->totalSize;
    }

    //Get current usage in bytes of the buffer
    size_t GetUsage() const
    {
        return allocator->allocatedSize;
    }

    //Check if an allocation of inSize can be made in this fixed buffer allocator
    bool CanAllocate(unsigned int inSize) const
    {
        return allocator->allocatedSize + memoryAlign(inSize, JPH_RVECTOR_ALIGNMENT) <= allocator->totalSize;
    }

    /// Check if memory block at inAddress is owned by this allocator
    bool OwnsMemory(const void* inAddress) const
    {
        return inAddress >= allocator->memory && inAddress < allocator->memory + allocator->totalSize;
    }

    StackAllocator* allocator;
};

struct Physics
{
    static Physics& instance() 
    {
        static Physics physics;
        return physics;
    }

    ~Physics() 
    {
        // Unregisters all types with the factory and cleans up the default material
        JPH::UnregisterTypes();

        // Destroy the factory
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
    PhysicAllocator tempAllocator;

    // The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
    // variant of this. We're going to use the locking version (even though we're not planning to access bodies from multiple threads)
    JPH::PhysicsSystem physicsSystem;
    // We need a job system that will execute physics jobs on multiple threads. Typically
    // you would implement the JobSystem interface yourself and let Jolt Physics run on top
    // of your own job scheduler. JobSystemThreadPool is an example implementation.
    JPH::JobSystemThreadPool jobSystem;
    JPH::BodyInterface* bodyInterface;

    uint32_t step = 0;

    // Create mapping table from object layer to broadphase layer
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    // Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
    BroadPhaseLayerImpl broadPhaseLayerInterface;

    // Create class that filters object vs broadphase layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    // Also have a look at ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for a simpler interface.
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphaseLayerFilter;

    // Create class that filters object vs object layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    // Also have a look at ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler interface.
    ObjectLayerPairFilterImpl objectVsObjectLayerFilter;

    // A body activation listener gets notified when bodies activate and go to sleep
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    MyBodyActivationListener bodyActivationListener;
    
    // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    VoidContactListener contactListener;


    // We simulate the physics world in discrete time steps. 60 Hz is a good rate to update the physics system.
    static constexpr float cDeltaTime = 1.0f / 60.0f;

	void updatePhysics(float delta);

	void shutdownPhysics();

private:
    Physics();
    Physics(const Physics& rhs) = delete;
    Physics(Physics&& rhs) = delete;
    void operator=(const Physics& rhs) = delete;
    void operator=(Physics&& rhs) = delete;

    void initPhysics();
};

#endif // !PHYSICS_HDR
