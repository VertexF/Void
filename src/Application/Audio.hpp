#ifndef AUDIO_HDR
#define AUDIO_HDR

#include "vender/miniaudio.h"

#include "Foundation/Array.hpp"

namespace sfx
{
	//#include "miniaudio.h"
	enum SFX_TYPE : uint32_t
	{
		FlyingNoise,
		Lazer,

		COUNT
	};

	static const char* sEffects[] =
	{
		"flyingNoise.flac", "lazer.flac", "COUNT"
	};

	static const char* toString(SFX_TYPE type)
	{
		return (static_cast<uint32_t>(type) < SFX_TYPE::COUNT ? sEffects[static_cast<int>(type)] : "Unsupported");
	}
}

namespace mus
{
	//#include "miniaudio.h"
	enum MUSIC_TYPE : uint32_t
	{
		Lufia2Battle,

		COUNT
	};

	static const char* sMusic[] =
	{
		"Lufia2Battle.flac", "COUNT"
	};

	static const char* toString(MUSIC_TYPE type)
	{
		return (static_cast<uint32_t>(type) < MUSIC_TYPE::COUNT ? sMusic[static_cast<int>(type)] : "Unsupported");
	}
}

struct AudioSystem
{
	void init();
	void playSoundEffect(sfx::SFX_TYPE type);
	void stopSoundEffect(sfx::SFX_TYPE type);
	void selectAudioDevice();
	void loadAudio();
	void shutdown();

	Array<ma_sound> soundEffects;
	Array<ma_sound> music;
};

#endif // !AUDIO_HDR
