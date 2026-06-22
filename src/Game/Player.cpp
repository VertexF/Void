#include "Player.hpp"

#include "Physics/Physics.hpp"

#include "Scene.hpp"

static constexpr float cCharacterRadiusStanding = 0.3f;

void Player::init(EntityData& entityData, const JPH::ShapeRefC& shapeRef, const JPH::Mat44& modelShape)
{
    playerSettings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    playerSettings.mLayer = Layers::CHARACTER;
    playerSettings.mShape = shapeRef;
    playerSettings.mFriction = 0.5f;
    playerSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cCharacterRadiusStanding);

    entityData.colour = { 1.f, 1.f, 1.f, 1.f };
    entityData.position = glms_mat4_identity();
    entityData.debugModel = convertToMat4(modelShape);

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

void Player::update(float deltaTime, AudioSystem& audio)
{
    //Update velocity
    JPH::Vec3 currentVelocity = character->GetLinearVelocity();
    JPH::Vec3 desiredVelocity = 2200.f * playerMovement;
    JPH::Vec3 newVelocity = 0.75f * currentVelocity + 0.25f * desiredVelocity * deltaTime;
        
    //Update the velocity
    character->SetLinearVelocity(newVelocity);

    if (soundEffect == true) 
    {
        //audio.playSoundEffect(sfx::Lazer);
        soundEffect = false;
    }
}

void Player::resetPosition()
{
    character->SetPosition({ 0.f, 0.f, 0.f });
}

void Player::crashNoise()
{
    soundEffect = true;
}