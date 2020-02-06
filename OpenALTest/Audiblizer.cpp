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
    processUnqueueableBuffersThread(nullptr),
    processUnqueueableBuffersThreadRunning(false),
    processUnqueueableBuffersThreadEvent(false, true),
    audioBufferMapDurationMilliseconds(0),
    initialized(false)
{
    
}

Audiblizer::~Audiblizer()
{
    if(processUnqueueableBuffersThread != nullptr)
    {
        processUnqueueableBuffersThreadRunning = false;
        processUnqueueableBuffersThreadEvent.Signal();
        
        processUnqueueableBuffersThread->join();
        delete processUnqueueableBuffersThread;
        processUnqueueableBuffersThread = nullptr;
    }
    
    if(source != 0)
    {
        Stop();
        
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

void Audiblizer::PrepareForDestruction()
{
    Stop();
    bufferCompletionListener = nullptr;
}

bool Audiblizer::Initialize()
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
    
    // init processedBuffers to be 1
    // --------------------------------------------------------------
    processBuffersCount = 1;
    processedBuffers = (ALuint*)malloc(sizeof(ALuint) * processBuffersCount);
    if(processedBuffers == nullptr)
    {
        error = AL_OUT_OF_MEMORY;
        goto CleanUp;
    }
    
    // start up the unqueueing thread
    // --------------------------------------------------------------
    processUnqueueableBuffersThreadRunning = true;
    processUnqueueableBuffersThread = new (std::nothrow) std::thread (ProcessUnqueueableBuffersThreadProc, this);
    if(processUnqueueableBuffersThread == nullptr)
    {
        error = AL_OUT_OF_MEMORY;
        processUnqueueableBuffersThreadRunning = false;
        goto CleanUp;
    }
    
    retVal = true;
    initialized = true;
CleanUp:
    if(error != AL_NO_ERROR)
    {
        if(processUnqueueableBuffersThread != nullptr)
        {
            processUnqueueableBuffersThreadRunning = false;
            processUnqueueableBuffersThreadEvent.Signal();
            
            processUnqueueableBuffersThread->join();
            delete processUnqueueableBuffersThread;
            processUnqueueableBuffersThread = nullptr;
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
        
        if(processedBuffers != nullptr)
        {
            free(processedBuffers);
            processedBuffers = nullptr;
            
            processBuffersCount = 0;
        }
    }
    
    return retVal;
}

void Audiblizer::SetBuffersCompletedListener(std::shared_ptr<BufferCompletionListener> listener)
{
    bufferCompletionListener = listener;
}

bool Audiblizer::QueueAudio(const AudioChunkVector &audioChunks)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return false;
    }
    
    ALCenum error = AL_NO_ERROR;
    bool retVal = true;
    ALuint buffer = 0;
    ALint sourceState = 0;
    AudioBufferMapInsertionPair audioBufferMapInsertionPair;
    uint64_t audioChunkDurationMilliseconds;
    
    for(uint32_t i = 0; i < audioChunks.size(); i++)
    {
        // ensure that the chunk has valid params
        if(audioChunks[i].format == AudioFormat_None ||
           audioChunks[i].buffer == nullptr ||
           audioChunks[i].bufferSize == 0 ||
           audioChunks[i].sampleRate == 0)
        {
            retVal = false;
            goto CleanUp;
        }
        
        // find the duration of this audio chunk in milliseconds
        // --------------------------------------------------------------
        audioChunkDurationMilliseconds = (audioChunks[i].bufferSize * 1000.0) / (AudioFormatFrameByteLength(audioChunks[i].format) * audioChunks[i].sampleRate);
        
        // generate and initialize sound buffer
        // --------------------------------------------------------------
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
        audioBufferMapInsertionPair = audioBufferMap.insert(AudioBufferMapPair(buffer, AudioBufferMapValue(audioChunks[i].buffer, audioChunkDurationMilliseconds)));
        if(!audioBufferMapInsertionPair.second)
        {
            retVal = false;
            goto CleanUp;
        }
        
        audioBufferMapDurationMilliseconds += audioChunkDurationMilliseconds;
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
        // start the unqueueing thread
        processUnqueueableBuffersThreadEvent.Signal();
        
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

uint32_t Audiblizer::NumBuffersQueued()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return false;
    }
    
    ALCenum error = AL_NO_ERROR;
    ALint numBuffersQueued;
    
    alGetSourcei(source, AL_BUFFERS_QUEUED, &numBuffersQueued);
    if (error != AL_NO_ERROR)
    {
        numBuffersQueued = UINT32_MAX;
    }
    
    return numBuffersQueued;
}

double Audiblizer::QueuedAudioDurationSeconds()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return 0;
    }
    
    return audioBufferMapDurationMilliseconds / 1000.0;
}

bool Audiblizer::Stop()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return false;
    }
    
    bool retVal = true;
    
    alSourceStop(source);
    
    // unbind all buffers that are still attached to source
    alSourcei(source, AL_BUFFER, NULL);
    
    // -----------
    // TODO - in the event that there is no bufferCompletionListener, who destroys any audio data bound to the source?
    // -----------
    
    // clear out the audioBufferMap
    audioBufferMap.clear();
    audioBufferMapDurationMilliseconds = 0;
    
    // pause the unqueueing thread
    processUnqueueableBuffersThreadEvent.Clear();
    
    return retVal;
}

bool Audiblizer::ProcessUnqueueableBuffers()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return false;
    }
    
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
                buffersCompleted.push_back(iter->second.audioBufferData);
            }
            // otherwise WE free() this memory
            else
            {
                if(iter->second.audioBufferData != nullptr)
                {
                    free(iter->second.audioBufferData);
                }
            }
            
            // lop off the duration of the unqueued buffer from the total
            if(audioBufferMapDurationMilliseconds > iter->second.audioBufferDurationMilliseconds)
            {
                audioBufferMapDurationMilliseconds -= iter->second.audioBufferDurationMilliseconds;
            }
            else
            {
                audioBufferMapDurationMilliseconds = 0;
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

void Audiblizer::ProcessUnqueueableBuffersThreadProc(Audiblizer *audiblizer)
{
    while(true)
    {
        audiblizer->processUnqueueableBuffersThreadEvent.Wait();
        
        if(!audiblizer->processUnqueueableBuffersThreadRunning)
        {
            break;
        }
        
        audiblizer->ProcessUnqueueableBuffers();
    }
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

uint32_t Audiblizer::AudioFormatFrameByteLength(AudioFormat audioFormat)
{
    uint32_t frameByteLength = 0;
    
    switch(audioFormat)
    {
        case AudioFormat_None:
            frameByteLength = 0;
            break;
        case AudioFormat_Mono8:
            frameByteLength = 1;
            break;
        case AudioFormat_Mono16:
            frameByteLength = 2;
            break;
        case AudioFormat_Stereo8:
            frameByteLength = 2;
            break;
        case AudioFormat_Stereo16:
            frameByteLength = 4;
            break;
    }
    
    return frameByteLength;
}

uint32_t Audiblizer::AudioFormatFrameDatumLength(AudioFormat audioFormat)
{
    uint32_t frameDatumLength = 0;
    
    switch(audioFormat)
    {
        case AudioFormat_None:
            frameDatumLength = 0;
            break;
        case AudioFormat_Mono8:
            frameDatumLength = 1;
            break;
        case AudioFormat_Mono16:
            frameDatumLength = 1;
            break;
        case AudioFormat_Stereo8:
            frameDatumLength = 2;
            break;
        case AudioFormat_Stereo16:
            frameDatumLength = 2;
            break;
    }
    
    return frameDatumLength;
}
