#ifndef CONTACT_LISTENER_HDR
#define CONTACT_LISTENER_HDR

#include "Foundation/Array.hpp"
#include "Foundation/ArrayThread.hpp"

#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Physics/Collision/ContactListener.h>


class VoidContactListener : public JPH::ContactListener
{
public:
    VoidContactListener();
    virtual ~VoidContactListener() = default;

    // See: ContactListener
    virtual JPH::ValidateResult	OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override;

    virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

    virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;

    ArrayThread<uint32_t> toDeleteQueue;
};

#endif // !CONTACT_LISTENER_HDR
