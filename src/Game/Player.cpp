#include "Player.hpp"

#include "Physics/Physics.hpp"

static constexpr float cCharacterRadiusStanding = 0.3f;

void Player::init()
{
    JPH::SphereShapeSettings playerSphereSettings{ 1.f };
    playerSphereSettings.SetEmbedded();

    JPH::ShapeSettings::ShapeResult playerShapeResult = playerSphereSettings.Create();
    JPH::ShapeRefC playerShapeRef = playerShapeResult.Get();

    playerSettings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    playerSettings.mLayer = Layers::MOVING;
    playerSettings.mShape = playerShapeRef;
    playerSettings.mFriction = 0.5f;
    playerSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cCharacterRadiusStanding);
    //TODO - change for your allocator, check object life.
    character = new JPH::Character{ &playerSettings, JPH::RVec3Arg::sZero(), JPH::QuatArg::sIdentity(), 0, &Physics::instance().physicsSystem };
    character->AddToPhysicsSystem(JPH::EActivation::Activate);
}

void Player::handleEvents(const InputHandler& input, const JPH::Vec3& cameraForwardVector)
{
    // Determine controller input
    playerMovement = JPH::Vec3::sZero();
    playerMovement.SetX(-1);

    if (input.isKeyDown(Keys::KEY_A))
    {
        playerMovement.SetZ(1);
    }

    if (input.isKeyDown(Keys::KEY_D))
    {
        playerMovement.SetZ(-1);
    }

    if (input.isKeyDown(Keys::KEY_S))
    {
        playerMovement.SetX(1);
    }

    if (playerMovement != JPH::Vec3::sZero())
    {
        playerMovement = playerMovement.Normalized();
    }

    // Rotate controls to align with the camera
    JPH::Vec3 camForward = cameraForwardVector;
    camForward = camForward.NormalizedOr(JPH::Vec3::sAxisX());
    JPH::Quat rotation = JPH::Quat::sFromTo(JPH::Vec3::sAxisX(), camForward);
    playerMovement = rotation * playerMovement;
}

void Player::update(float deltaTime)
{
    //Cancel movement in opposite direction of normal when touching something we can't walk up
    JPH::Vec3 normal = character->GetGroundNormal();
    float dot = normal.Dot(playerMovement);
    if (dot < 0.0f)
    {
        playerMovement -= (dot * normal) / normal.LengthSq();
    }

    //Update velocity
    JPH::Vec3 currentVelocity = character->GetLinearVelocity();
    JPH::Vec3 desiredVelocity = 2200.f * playerMovement;
    JPH::Vec3 newVelocity = 0.75f * currentVelocity + 0.25f * desiredVelocity * deltaTime;
        
    //Update the velocity
    character->SetLinearVelocity(newVelocity);
}

void Player::resetPosition()
{
    character->SetPosition({ 0.f, 0.f, 0.f });
}