#include "Audio.hpp"

#include "Foundation/Platform.hpp"
#include "Foundation/Assert.hpp"
#include "Foundation/Numerics.hpp"
#include "Foundation/Memory.hpp"
#include "Foundation/String.hpp"

namespace 
{
	ma_engine sEngines;
    ma_device sDevices;

    ma_resource_manager_config resourceManagerConfig;
    ma_resource_manager resourceManager;
    ma_context context;
    ma_device_info* playbackDeviceInfos;
    ma_uint32 playbackDeviceCount = 0;

    ma_sound musicSoundGroup;
    ma_sound effectsSoundGroup;

    void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
    {
        (void)pInput;

        /*
        Since we're managing the underlying device ourselves, we need to read from the engine directly.
        To do this we need access to the ma_engine object which we passed in to the user data. One
        advantage of this is that you could do your own audio processing in addition to the engine's
        standard processing.
        */
        ma_engine_read_pcm_frames((ma_engine*)pDevice->pUserData, pOutput, frameCount, NULL);
    }
}

void AudioSystem::init() 
{
    soundEffects.init(&MemoryService::instance()->systemAllocator, 16, 16);
    music.init(&MemoryService::instance()->systemAllocator, 8, 8);

    resourceManagerConfig = ma_resource_manager_config_init();
    resourceManagerConfig.decodedFormat = ma_format_f32;
    resourceManagerConfig.decodedChannels = 0;
    resourceManagerConfig.decodedSampleRate = 48000;

    ma_result result = ma_resource_manager_init(&resourceManagerConfig, &resourceManager);
    VOID_ASSERTM(result == MA_SUCCESS, "Failed to create the resoure manager.\n");

    result = ma_context_init(nullptr, 0, nullptr, &context);
    VOID_ASSERTM(result == MA_SUCCESS, "Failed to create the context.\n");

    result = ma_context_get_devices(&context, &playbackDeviceInfos, &playbackDeviceCount, nullptr, nullptr);
    VOID_ASSERTM(result == MA_SUCCESS, "Failed to enumerate the deviced.\n");

    //Note: If you change audio engine at runtime we break all the audio that has already been loaded.
    result = ma_engine_init(NULL, &sEngines);
    VOID_ASSERTM(result == MA_SUCCESS, "Failed to initialize audio engine.\n");
}

void AudioSystem::playSoundEffect(sfx::SFX_TYPE type)
{
    ma_sound_start(&soundEffects[type]);
}

void AudioSystem::selectAudioDevice()
{
    /* We have our devices, so now we want to get the user to select the devices they want to output to. */
    ma_result result;
    int c = 0;
    //TODO: Move into the engine rather than at standard out.
    for (;;)
    {
        vprint("Select playback device %d ([%d - %d], Q to quit):\n", 1, 0, min((int)playbackDeviceCount, 9));

        for (ma_uint32 iAvailableDevice = 0; iAvailableDevice < playbackDeviceCount; ++iAvailableDevice)
        {
            vprint("    %d: %s\n", iAvailableDevice, playbackDeviceInfos[iAvailableDevice].name);
        }

        for (;;)
        {
            c = getchar();
            if (c != '\n')
            {
                break;
            }
        }

        if (c == 'q' || c == 'Q')
        {
            return;   /* User aborted. */
        }

        if (c >= '0' && c <= '9')
        {
            c -= '0';

            if (c < (int)playbackDeviceCount)
            {
                ma_device_config deviceConfig;
                ma_engine_config engineConfig;

                /*
                Create the device first before the engine. We'll specify the device in the engine's config. This is optional. When a device is
                not pre-initialized the engine will create one for you internally. The device does not need to be started here - the engine will
                do that for us in `ma_engine_start()`. The device's format is derived from the resource manager, but can be whatever you want.
                It's useful to keep the format consistent with the resource manager to avoid data conversions costs in the audio callback. In
                this example we're using the resource manager's sample format and sample rate, but leaving the channel count set to the device's
                native channels. You can use whatever format/channels/rate you like.
                */
                deviceConfig = ma_device_config_init(ma_device_type_playback);
                deviceConfig.playback.pDeviceID = &playbackDeviceInfos[c].id;
                deviceConfig.playback.format = resourceManager.config.decodedFormat;
                deviceConfig.playback.channels = 0;
                deviceConfig.sampleRate = resourceManager.config.decodedSampleRate;
                deviceConfig.dataCallback = dataCallback;
                deviceConfig.pUserData = &sEngines;

                result = ma_device_init(&context, &deviceConfig, &sDevices);
                VOID_ASSERTM(result == MA_SUCCESS, "Could not initalise the audio devices.\n");

                /* Now that we have the device we can initialize the engine. The device is passed into the engine's config. */
                engineConfig = ma_engine_config_init();
                engineConfig.pDevice = &sDevices;
                engineConfig.pResourceManager = &resourceManager;
                engineConfig.noAutoStart = MA_TRUE;    /* Don't start the engine by default - we'll do that manually below. */

                result = ma_engine_init(&engineConfig, &sEngines);
                if (result != MA_SUCCESS)
                {
                    ma_device_uninit(&sDevices);
                    VOID_ASSERTM(result == MA_SUCCESS, "Could not initalise the audio engine.\n");
                }

                break;
            }
            else
            {
                vprint("Invalid device number.\n");
            }
        }
        else
        {
            vprint("Invalid device number.\n");
        }
    }

    vprint("Device %d: %s\n", 0, playbackDeviceInfos[c].name);

    /* We should now have our engine's initialized. We can now start them. */
    result = ma_engine_start(&sEngines);
    VOID_ASSERTM(result == MA_SUCCESS, "Could not initalise the engine.\n");
}

void AudioSystem::loadAudio()
{
    ma_result result = ma_sound_group_init(&sEngines, 0, nullptr, &musicSoundGroup);
    VOID_ASSERTM(result == MA_SUCCESS, "Failed to create music sound group.\n");
    ma_sound_set_volume(&musicSoundGroup, 0.2);

    result = ma_sound_group_init(&sEngines, 0, nullptr, &effectsSoundGroup);
    VOID_ASSERTM(result == MA_SUCCESS, "Failed to create effects sound group.\n");
    ma_sound_set_volume(&effectsSoundGroup, 0.4);

    const char* basePath = "Assets/Audio/%s";
    StringBuffer fullAddress;
    fullAddress.init(1048, &MemoryService::instance()->scratchAllocator);
    uint32_t musicMarker = MemoryService::instance()->scratchAllocator.getMarker();
    for (uint32_t i = 0; i < sfx::COUNT; ++i) 
    {
        const char* fullpath = fullAddress.appendUseF(basePath, sfx::toString((sfx::SFX_TYPE)i));

        result = ma_sound_init_from_file(&sEngines, fullpath, 0, &musicSoundGroup, NULL, &soundEffects[i]);
        VOID_ASSERTM(result == MA_SUCCESS, "Could not load file %s\n", fullpath);
    }
    MemoryService::instance()->scratchAllocator.freeMarker(musicMarker);

    musicMarker = MemoryService::instance()->scratchAllocator.getMarker();
    for (uint32_t i = 0; i < mus::COUNT; ++i)
    {
        const char* fullpath = fullAddress.appendUseF(basePath, mus::toString((mus::MUSIC_TYPE)i));

        result = ma_sound_init_from_file(&sEngines, fullpath, 0, &musicSoundGroup, NULL, &music[i]);
        VOID_ASSERTM(result == MA_SUCCESS, "Could not load file %s\n", fullpath);
    }
    MemoryService::instance()->scratchAllocator.freeMarker(musicMarker);
    fullAddress.shutdown();

    ma_sound_set_looping(&music[mus::Lufia2Battle], MA_TRUE);
    ma_sound_start(&music[mus::Lufia2Battle]);

    ma_sound_set_looping(&soundEffects[sfx::FlyingNoise], MA_TRUE);
    ma_sound_start(&soundEffects[sfx::FlyingNoise]);
}

void AudioSystem::shutdown()
{
    for (uint32_t i = 0; i < sfx::COUNT; ++i)
    {
        ma_sound_uninit(&soundEffects[i]);
    }

    for (uint32_t i = 0; i < mus::COUNT; ++i)
    {
        ma_sound_uninit(&music[i]);
    }

    soundEffects.shutdown();
    music.shutdown();

    ma_engine_uninit(&sEngines);

    ma_context_uninit(&context);

    ma_resource_manager_uninit(&resourceManager);
}