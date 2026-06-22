#include "Physics.hpp"

#include "Foundation/Memory.hpp"

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2025 Jorrit Rouwe
// SPDX-License-Identifier: CC0-1.0
// This file is in the public domain. It serves as an example to start building your own application using Jolt Physics. Feel free to copy paste without attribution!

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
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

// STL includes
#include <thread>
#include <cstdarg>
#include <iostream>

namespace
{
    // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
    static constexpr uint32_t cMaxBodies = 65536;

    // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
    static constexpr uint32_t cNumBodyMutexes = 0;

    // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
    // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
    // too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
    static constexpr uint32_t cMaxBodyPairs = 65536;

    // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
    // number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 1024.
    static constexpr uint32_t cMaxContactConstraints = 1024;
}

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char* inFMT, ...)
{
    // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    // Print to the TTY
    vprint(buffer);
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine)
{
    // Print to the TTY
    vprint("%s : %d : ( %s) %s", inFile, inLine, inExpression, (inMessage != nullptr ? inMessage : ""));

    // Breakpoint
    return true;
};

#endif // JPH_ENABLE_ASSERTS

Physics::Physics() 
{
    initPhysics();
}

void Physics::initPhysics()
{
    // Install trace and assert callbacks
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    toDeleteQueue.init(&MemoryService::instance()->systemAllocator, 4);

    // Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
    // It is not directly used in this example but still required.
    JPH::Factory::sInstance = new JPH::Factory();

    // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
    // If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
    // If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
    JPH::RegisterTypes();

    // We need a job system that will execute physics jobs on multiple threads. Typically
    // you would implement the JobSystem interface yourself and let Jolt Physics run on top
    // of your own job scheduler. JobSystemThreadPool is an example implementation.
    jobSystem.Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

    physicsSystem.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broadPhaseLayerInterface, objectVsBroadphaseLayerFilter, objectVsObjectLayerFilter);

    physicsSystem.SetGravity({0.f, 0.f, 0.f});
    physicsSystem.SetBodyActivationListener(&bodyActivationListener);

    physicsSystem.SetContactListener(&contactListener);

    // The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
    // variant of this. We're going to use the locking version (even though we're not planning to access bodies from multiple threads)
    bodyInterface = physicsSystem.GetBodyInterfacePointer();

    // Now we're ready to simulate the body, keep simulating until it goes to sleep
    step = 0;
}

void Physics::updatePhysics(float delta)
{
    // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. 
    // Do 1 collision step per 1 / 60th of a second (round up).
    int cCollisionSteps = 1;

    // Step the world
    physicsSystem.Update(cDeltaTime, cCollisionSteps, &tempAllocator, &jobSystem);
}

void Physics::shutdownPhysics()
{
    //// Unregisters all types with the factory and cleans up the default material
    //JPH::UnregisterTypes();

    //// Destroy the factory
    //delete JPH::Factory::sInstance;
    //JPH::Factory::sInstance = nullptr;

    toDeleteQueue.shutdown();
}