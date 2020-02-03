//
//  main.cpp
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/3/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#include <iostream>
#include <string>
#include <chrono>
#include <map>
#include <iterator>
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
    ALint numBuffersProcessed = 0;
    ALuint processedBuffers[10]; // should be dynamically sized...
    uint32_t sampleRate = 48000;
    double durationSeconds = 1.0;
    bool stereo = true;
    uint16_t *audioData = nullptr;
    size_t audioDataSize = 0;
    ALint sourceState = 0;
    std::string sourceStateString;
    
    typedef ALuint AudioBufferMapKey; // Buffer ID
    typedef uint16_t* AudioBufferMapValue; // Buffer Data
    typedef std::map<AudioBufferMapKey, AudioBufferMapValue> AudioBufferMap;
    typedef std::pair<AudioBufferMapKey, AudioBufferMapValue> AudioBufferMapPair;
    typedef AudioBufferMap::iterator AudioBufferMapIterator;
    typedef std::pair<AudioBufferMapIterator, bool> AudioBufferMapInsertionPair;
    
    AudioBufferMap audioBufferMap;
    AudioBufferMapInsertionPair audioBufferMapInsertionPair;
    
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
    
    // Queue sound buffer onto source
    // --------------------------------------------------------------
    alSourceQueueBuffers(source, 1, &buffer);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: alSourceQueueBuffers!!!\n");
        goto CleanUp;
    }
    
    printf("AudioData Queued -- bufferID:%d  audioDataPtr:%p\n", buffer, audioData);
    
    // insert buffer into audioBufferMap
    // --------------------------------------------------------------
    audioBufferMapInsertionPair = audioBufferMap.insert(AudioBufferMapPair(buffer, audioData));
    if(!audioBufferMapInsertionPair.second)
    {
        printf("ERROR: audioBufferMap.insert!!!\n");
        goto CleanUp;
    }
    
    // start playing source
    // --------------------------------------------------------------
    alSourcePlay(source);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: SourcePlay!!!\n");
        goto CleanUp;
    }
    
    time0 = std::chrono::high_resolution_clock::now();
    
    // test to see when queued buffer is done playing
    // --------------------------------------------------------------
    numBuffersProcessed = 0;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &numBuffersProcessed);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: GetNumBuffersProcessed!!!\n");
        goto CleanUp;
    }
    
    while(numBuffersProcessed == 0)
    {
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &numBuffersProcessed);
        if (error != AL_NO_ERROR)
        {
            printf("ERROR: GetNumBuffersProcessed!!!\n");
            goto CleanUp;
        }
    }
    
    // unqueue the buffer
    // --------------------------------------------------------------
    time1 = std::chrono::high_resolution_clock::now();
    
    alSourceUnqueueBuffers(source, numBuffersProcessed, processedBuffers);
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: SourceUnqueueBuffers!!!\n");
        goto CleanUp;
    }
    
    printf("Unqueueing buffers...\n");
    for(uint32_t bufferIter = 0; bufferIter < numBuffersProcessed; bufferIter++)
    {
        // see if the buffer is in the audioBufferMap
        AudioBufferMapIterator iter = audioBufferMap.find(processedBuffers[bufferIter]);
        if(iter != audioBufferMap.end())
        {
            printf("Buffer IN map: bufferID:%d  audioDataPtr:%p\n", iter->first, iter->second);
            
            // remove buffer from map
            audioBufferMap.erase(iter);
            
            // Note: Normally we would free() the buffer here (if desired), but due to the
            //       simplicity of the example, we free the buffer at the end of main()
        }
        else
        {
            printf("Buffer NOT map: bufferID:%d", processedBuffers[bufferIter]);
        }
    }
    printf("\n");
    
    // check the state of the source
    // --------------------------------------------------------------
    alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        printf("ERROR: GetSourceState!!!\n");
        goto CleanUp;
    }
    
    switch(sourceState)
    {
        case AL_INITIAL:
            sourceStateString = "Initial";
            break;
        case AL_PLAYING:
            sourceStateString = "Playing";
            break;
        case AL_PAUSED:
            sourceStateString = "Paused";
            break;
        case AL_STOPPED:
            sourceStateString = "Stopped";
            break;
        default:
            sourceStateString = "UNKNOWN!!!";
            break;
    }
    
    printf("SourceState: %s\n", sourceStateString.c_str());
    
    // report results
    // --------------------------------------------------------------
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
