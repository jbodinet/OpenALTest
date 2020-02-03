//
//  main.cpp
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/3/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#include <iostream>
#include <chrono>
#include <OpenAL/al.h>
#include <OpenAL/alc.h>

void ListAudioDevices(const ALCchar *devices);
uint16_t* GenerateAudioSample(uint32_t sampleRate, double durationSeconds, bool stereo, size_t *bufferSize = nullptr);
void FreeAudioSample(uint16_t* data);

int main(int argc, const char * argv[]) {
    ALCenum error = AL_NO_ERROR;
    ALCdevice *device = nullptr;
    ALCcontext *context = nullptr;
    ALuint source = 0;
    ALuint buffer = 0;
    uint32_t sampleRate = 48000;
    double durationSeconds = 1.0;
    bool stereo = true;
    uint16_t *audioData = nullptr;
    size_t audioDataSize = 0;
    ALint sourceState = 0;
    
    std::chrono::high_resolution_clock::time_point time0, time1;
    std::chrono::duration<float> floatingPointSeconds;
    std::chrono::milliseconds milliseconds;
    
    // create device
    // ------------------------------------------------------------
    device = alcOpenDevice(NULL);
    if (!device)
    {
        printf("ERROR: OpenDevice!!!\n");
        goto CleanUp;
    }
    
    ListAudioDevices(alcGetString(NULL, ALC_DEVICE_SPECIFIER));
    
    // create context
    // -------------------------------------------------------------
    context = alcCreateContext(device, NULL);
    if (!alcMakeContextCurrent(context))
    {
        printf("ERROR: CreateContext!!!\n");
        goto CleanUp;
    }
    
    // create sound source
    // --------------------------------------------------------------
    alGenSources((ALuint)1, &source);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: GenSources!!!\n");
        goto CleanUp;
    }
    
    // configure sound source
    // --------------------------------------------------------------
    alSourcef(source, AL_PITCH, 1);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: Configure Pitch!!!\n");
        goto CleanUp;
    }
    
    alSourcef(source, AL_GAIN, 1);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: Configure Gain!!!\n");
        goto CleanUp;
    }

    alSource3f(source, AL_POSITION, 0, 0, 0);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: Configure Position!!!\n");
        goto CleanUp;
    }
    
    alSource3f(source, AL_VELOCITY, 0, 0, 0);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: Configure Velocity!!!\n");
        goto CleanUp;
    }
    
    alSourcei(source, AL_LOOPING, AL_FALSE);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: Configure Looping!!!\n");
        goto CleanUp;
    }
    
    // generate sample sound
    // --------------------------------------------------------------
    audioData = GenerateAudioSample(sampleRate, durationSeconds, stereo, &audioDataSize);
    if(audioData == nullptr)
    {
        printf("ERROR: Generating Audio Sample Data!!!\n");
        goto CleanUp;
    }
    
    // generate / load audio buffer
    // --------------------------------------------------------------
    alGenBuffers((ALuint)1, &buffer);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: GenBuffers!!!\n");
        goto CleanUp;
    }
    
    alBufferData(buffer, AL_FORMAT_STEREO16, audioData, (ALsizei)audioDataSize, (ALsizei)sampleRate);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: BufferData!!!\n");
        goto CleanUp;
    }
    
    // bind sound buffer to source
    // --------------------------------------------------------------
    alSourcei(source, AL_BUFFER, buffer);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: Bind Buffer to Source!!!\n");
        goto CleanUp;
    }
    
    // start playing source
    // --------------------------------------------------------------
    time0 = std::chrono::high_resolution_clock::now();
    
    alSourcePlay(source);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: SourcePlay!!!\n");
        goto CleanUp;
    }
    
    alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: GetSourceState!!!\n");
        goto CleanUp;
    }
    
    while (sourceState == AL_PLAYING) {
        alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
        if (error != AL_NO_ERROR)
        {
            printf("ERROR: GetSourceState!!!\n");
            goto CleanUp;
        }
    }
    
    // report results
    // --------------------------------------------------------------
    time1 = std::chrono::high_resolution_clock::now();
    floatingPointSeconds = time1 - time0;
    milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(floatingPointSeconds);
    printf("Audio Duration Time sec:%f  ms:%lld\n", floatingPointSeconds.count(), milliseconds.count());
    
    printf("OpenAL Test Completed Successfully!!!\n");
CleanUp:
    if(buffer != 0)
    {
        alDeleteBuffers(1, &buffer);
        buffer = 0;
    }
    
    if(source != 0)
    {
        alDeleteSources(1, &source);
        source = 0;
    }
    
    if(context != nullptr)
    {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(context);
        context = nullptr;
    }
    
    if(device != nullptr)
    {
        alcCloseDevice(device);
        device = nullptr;
    }
    
    FreeAudioSample(audioData);
    audioData = nullptr;
        
    return 0;
}

void ListAudioDevices(const ALCchar *devices)
{
    const ALCchar *device = devices, *next = devices + 1;
    size_t len = 0;
    
    fprintf(stdout, "Devices list:\n");
    fprintf(stdout, "----------\n");
    while (device && *device != '\0' && next && *next != '\0') {
        fprintf(stdout, "%s\n", device);
        len = strlen(device);
        device += (len + 1);
        next += (len + 2);
    }
    fprintf(stdout, "----------\n");
}

uint16_t* GenerateAudioSample(uint32_t sampleRate, double durationSeconds, bool stereo, size_t *bufferSizeOut)
{
    int32_t bufferChannelFrames = int32_t((sampleRate * durationSeconds) + 0.5);
    int32_t numChannels = stereo ? 2 : 1;
    size_t bufferSize = bufferChannelFrames * numChannels * sizeof(uint16_t);
    uint16_t *buffer = (uint16_t*)malloc(bufferSize);
    if(buffer == nullptr)
    {
        if(bufferSizeOut != nullptr)
        {
            *bufferSizeOut = 0;
        }
        
        goto Exit;
    }
    
    for (uint32_t i = 0; i < bufferChannelFrames * numChannels; i += 2)
    {
        buffer[i] = 32768 - ((i % 100) * 660);
        buffer[i+1] = buffer[i];
    }
    
    if(bufferSizeOut != nullptr)
    {
        *bufferSizeOut = bufferSize;
    }
    
Exit:
    return buffer;
}

void FreeAudioSample(uint16_t* data)
{
    if(data != nullptr)
    {
        free(data);
        data = nullptr;
    }
}
