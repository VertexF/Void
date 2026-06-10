#ifndef PLAYER_HDR
#define PLAYER_HDR

#include "cglm/struct/vec3.h"

#include "Application/Input.hpp"
#include "Application/Audio.hpp"

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

//TODO - Be careful after the players ask yourself do you need JOLT in the actual player struct.
//You might want to abstract it away so you don't ended up with a clucky struct.
//Thick about how you don't want to write implementation code in UI for example. This might be the same case.

#include "Utils.hpp"

struct Player 
{
	void init(DebugEntityData& debugEntityData);
	void handleEvents(const InputHandler& input, const JPH::Vec3& cameraForwardVector);
	void update(float deltaTime, AudioSystem& audio);
	void resetPosition();
	void crashNoise();
	JPH::Vec3 playerMovement;
	JPH::CharacterSettings playerSettings;
	JPH::Character* character;

	bool soundEffect = false;
};


#endif // !PLAYER_HDR
