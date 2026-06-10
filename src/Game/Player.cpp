#include "Player.hpp"

#include "Physics/Physics.hpp"

#include "Scene.hpp"

static constexpr float cCharacterRadiusStanding = 0.3f;

void Player::init(EntityData& entityData)
{
    JPH::SphereShapeSettings playerSphereSettings{ 1.0f };
    playerSphereSettings.SetEmbedded();

    JPH::ShapeSettings::ShapeResult playerShapeResult = playerSphereSettings.Create();
    JPH::ShapeRefC playerShapeRef = playerShapeResult.Get();

    playerSettings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    playerSettings.mLayer = Layers::CHARACTER;
    playerSettings.mShape = playerShapeRef;
    playerSettings.mFriction = 0.5f;
    playerSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cCharacterRadiusStanding);

    vec3s playerInitPostion{ 0.f, 0.f, 0.f };
    JPH::BodyCreationSettings debugGeometryForPlayer{};
    debugGeometryForPlayer.SetShape(playerShapeRef);
    debugGeometryForPlayer.mPosition = JPH::Vec3Arg{ playerInitPostion.x, playerInitPostion.y, playerInitPostion.z };
    debugGeometryForPlayer.mRotation = JPH::Quat::sIdentity();
    debugGeometryForPlayer.mMotionType = JPH::EMotionType::Dynamic;
    debugGeometryForPlayer.mObjectLayer = Layers::MOVING;

    JPH::EShapeSubType shapeType = debugGeometryForPlayer.GetShape()->GetSubType();
    JPH::RMat44 shapeModel;
    switch (shapeType)
    {
    case JPH::EShapeSubType::Sphere:
    {
        shapeModel = JPH::RMat44::sScale(((JPH::SphereShape*)debugGeometryForPlayer.GetShape())->GetRadius());
    }
    break;
    default:
        VOID_ERROR("Shape type not supported.\n");
    }

    entityData.colour = { 1.f, 1.f, 1.f, 1.f };
    entityData.position = glms_mat4_identity();
    entityData.debugModel = convertToMat4(shapeModel);

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
        audio.playSoundEffect(sfx::Lazer);
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