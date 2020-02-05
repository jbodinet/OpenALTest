//
//  Audiblizer.cpp
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/5/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#include "Audiblizer.h"

Audiblizer::Audiblizer() :
    device(nullptr),
    context(nullptr),
    source(0),
    bufferCompletionListener(nullptr),
    processedBuffers(nullptr),
    processBuffersCount(0),
    initialized(false)
{
    
}

Audiblizer::~Audiblizer()
{
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
    
    if(processedBuffers != nullptr)
    {
        free(processedBuffers);
        processedBuffers = nullptr;
        
        processBuffersCount = 0;
    }
}

bool Audiblizer::Initialize(BufferCompletionListener *listener)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(initialized)
    {
        // can't reinit w/o explicitly tearing down
        return false;
    }
    
    ALCenum error = AL_NO_ERROR;
    bool retVal = false;
    
    // create device
    // ------------------------------------------------------------
    device = alcOpenDevice(NULL);
    if (!device)
    {
        printf("ERROR: OpenDevice!!!\n");
        goto CleanUp;
    }
    
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

    // hold on to the listener
    // --------------------------------------------------------------
    bufferCompletionListener = listener;
    
    // init processedBuffers to be 1
    // --------------------------------------------------------------
    processBuffersCount = 1;
    processedBuffers = (ALuint*)malloc(sizeof(ALuint) * processBuffersCount);
    
    retVal = true;
    initialized = true;
CleanUp:
    if(error != AL_NO_ERROR)
    {
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
    }
    
    return retVal;
}

bool Audiblizer::QueueAudio(const AudioChunkVector &audioChunks)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    ALCenum error = AL_NO_ERROR;
    bool retVal = true;
    ALuint buffer = 0;
    ALint sourceState = 0;
    AudioBufferMapInsertionPair audioBufferMapInsertionPair;
    
    for(uint32_t i = 0; i < audioChunks.size(); i++)
    {
        alGenBuffers((ALuint)1, &buffer);
        error = alGetError();
        if (error != AL_NO_ERROR)
        {
            retVal = false;
            goto CleanUp;
        }
        
        alBufferData(buffer, OpenALAudioFormat(audioChunks[i].format), audioChunks[i].buffer, (ALsizei)audioChunks[i].bufferSize, (ALsizei)audioChunks[i].sampleRate);
        error = alGetError();
        if (error != AL_NO_ERROR)
        {
            retVal = false;
            goto CleanUp;
        }
        
        // Queue sound buffer onto source
        // --------------------------------------------------------------
        alSourceQueueBuffers(source, 1, &buffer);
        error = alGetError();
        if (error != AL_NO_ERROR)
        {
            retVal = false;
            goto CleanUp;
        }
        
        // insert buffer into audioBufferMap
        // --------------------------------------------------------------
        audioBufferMapInsertionPair = audioBufferMap.insert(AudioBufferMapPair(buffer, audioChunks[i].buffer));
        if(!audioBufferMapInsertionPair.second)
        {
            retVal = false;
            goto CleanUp;
        }
    }
    
    // ensure that the source is playing
    // --------------------------------------------------------------
    alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        retVal = false;
        goto CleanUp;
    }
    
    if(sourceState != AL_PLAYING)
    {
        alSourcePlay(source);
        error = alGetError();
        if (error != AL_NO_ERROR)
        {
            retVal = false;
            goto CleanUp;
        }
    }
    
CleanUp:
    
    return retVal;
}

bool Audiblizer::Stop()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    bool retVal = true;
    
    alSourceStop(source);
    
    // unbind all buffers that are still attached to source
    alSourcei(source, AL_BUFFER, NULL);
    
    // clear out the audioBufferMap
    audioBufferMap.clear();
    
    return retVal;
}

bool Audiblizer::ProcessUnqueueableBuffers()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    bool retVal = true;
    ALCenum error = AL_NO_ERROR;
    ALint numBuffersProcessed = 0;
    BufferCompletionListener::BuffersCompletedVector buffersCompleted;
    
    // find out how many buffers have been processed
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &numBuffersProcessed);
    error = alGetError();
    if (error != AL_NO_ERROR)
    {
        retVal = false;
        goto Exit;
    }
    
    // if there are no buffers to process, skip to Exit
    if(numBuffersProcessed == 0)
    {
        goto Exit;
    }
    
    // ensure that there are enough elements in processedBuffers to get all of the buffers to be unqueued
    if(numBuffersProcessed > processBuffersCount)
    {
        if(processedBuffers != nullptr)
        {
            free(processedBuffers);
            processedBuffers = nullptr;
            
            processBuffersCount = 0;
        }
        
        processedBuffers = (ALuint*) malloc(sizeof(ALuint) * numBuffersProcessed);
        if(processedBuffers == nullptr)
        {
            retVal = false;
            goto Exit;
        }
        
        processBuffersCount = numBuffersProcessed;
    }
    
    // unqueue the buffers
    alSourceUnqueueBuffers(source, numBuffersProcessed, processedBuffers);
    if (error != AL_NO_ERROR)
    {
        retVal = false;
        goto Exit;
    }
    
    // handle the unqueued buffers
    for(uint32_t i = 0; i < numBuffersProcessed; i++)
    {
        AudioBufferMapIterator iter = audioBufferMap.find(processedBuffers[i]);
        if(iter != audioBufferMap.end())
        {
            // if there is a listener, the listener is responsible for freeing this memory,
            // so insert the dataPtr into the buffersCompleted vector
            if(bufferCompletionListener != nullptr)
            {
                buffersCompleted.push_back(iter->second);
            }
            // otherwise WE free() this memory
            else
            {
                if(iter->second != nullptr)
                {
                    free(iter->second);
                }
            }
            
            // remove buffer from map
            audioBufferMap.erase(iter);
        }
    }
    
    // call the bufferCompletion listener
    if(bufferCompletionListener != nullptr)
    {
        bufferCompletionListener->BuffersCompleted(buffersCompleted);
    }
    
    // delete the buffers
    alDeleteBuffers(numBuffersProcessed, processedBuffers);
    
Exit:
    return retVal;
}

ALenum Audiblizer::OpenALAudioFormat(AudioFormat audioFormat)
{
    ALenum openALEnum = (ALenum)0;
    
    switch(audioFormat)
    {
        case AudioFormat_None:
            openALEnum = (ALenum)0;
            break;
        case AudioFormat_Mono8:
            openALEnum = (ALenum)AL_FORMAT_MONO8;
            break;
        case AudioFormat_Mono16:
            openALEnum = (ALenum)AL_FORMAT_MONO16;
            break;
        case AudioFormat_Stereo8:
            openALEnum = (ALenum)AL_FORMAT_STEREO8;
            break;
        case AudioFormat_Stereo16:
            openALEnum = (ALenum)AL_FORMAT_STEREO16;
            break;
    }
    
    return openALEnum;
}
