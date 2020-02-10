//
//  AudiblizerTestHarness.cpp
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/5/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#include "AudiblizerTestHarness.h"

const Audiblizer::AudioFormat AudiblizerTestHarness::audioFormat = Audiblizer::AudioFormat_Stereo16;

AudiblizerTestHarness::AudiblizerTestHarness() :
    audioData(nullptr),
    audioDataPtr(nullptr),
    audioDataSize(0),
    audioDataTotalNumDatums(0),
    audioDataTotalNumFrames(0),
    firstCallToBuffersCompleted(false),
    audiblizer(nullptr),
    maxDelta(std::chrono::duration<float>::zero()),
    cumulativeDelta(std::chrono::duration<float>::zero()),
    numBuffersCompleted(0),
    audioQueueingThread(nullptr),
    audioQueueingThreadRunning(false),
    audioQueueingThreadTerminated(false, false),
    audioSampleRate(48000),
    audioIsStereo(true),
    audioDurationSeconds(5.0),
    maxQueuedAudioDurationSeconds(4.0),
    dataOutputThread(nullptr),
    dataOutputThreadRunning(false),
    initialized(false)
{
    
}

AudiblizerTestHarness::~AudiblizerTestHarness()
{
    StopTest();
    FreeAudioSample(audioData);
}

bool AudiblizerTestHarness::Initialize()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(initialized)
    {
        return false;
    }
    
    bool retVal = true;
    
    audiblizer = std::make_shared<Audiblizer>();
    if(!audiblizer->Initialize())
    {
        audiblizer = nullptr;
        retVal = false;
        
        goto Exit;
    }
    
    audiblizer->SetBuffersCompletedListener(getptr());
    
    FreeAudioSample(audioData);
    audioData = (uint8_t*)GenerateAudioSample(audioSampleRate, audioDurationSeconds, audioIsStereo, &audioDataSize);
    audioDataPtr = audioData;
    audioDataTotalNumDatums = audioDataSize / sizeof(uint16_t);
    audioDataTotalNumFrames = audioDataSize / Audiblizer::AudioFormatFrameByteLength(audioFormat);
    
    initialized = true;
    
Exit:
    return retVal;
}

void AudiblizerTestHarness::PrepareForDestruction()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return;
    }
    
    StopTest();
    audiblizer->PrepareForDestruction();
    audiblizer = nullptr;
    initialized = false;
}

bool AudiblizerTestHarness::StartTest(const VideoSegments &videoSegmentsArg)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return false;
    }
    
    if(audioQueueingThread != nullptr)
    {
        return false;
    }
    
    videoSegments = videoSegmentsArg;
    firstCallToBuffersCompleted = false;
    maxDelta = std::chrono::duration<float>::zero();
    cumulativeDelta = std::chrono::duration<float>::zero();
    numBuffersCompleted = 0;
    audioDataPtr = audioData;
    
    audioQueueingThreadRunning = true;
    audioQueueingThread = new (std::nothrow) std::thread(AudioQueueingThreadProc, this);
    if(audioQueueingThread == nullptr)
    {
        audioQueueingThreadRunning = false;
        return false;
    }
    
    dataOutputThreadRunning = true;
    dataOutputThread = new (std::nothrow) std::thread(DataOutputThreadProc, this);
    if(dataOutputThread == nullptr)
    {
        dataOutputThreadRunning = false;
        return false;
    }
        
    return true;
}

bool AudiblizerTestHarness::StopTest()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return false;
    }
    
    // stop the threads
    
    if(audioQueueingThread != nullptr)
    {
        audioQueueingThreadRunning = false;
        audioQueueingThread->join();
        delete audioQueueingThread;
        audioQueueingThread = nullptr;
    }
    
    if(dataOutputThread != nullptr)
    {
        dataOutputThreadRunning = false;
        dataOutputThread->join();
        delete dataOutputThread;
        dataOutputThread = nullptr;
    }
    
    // report average delta and max delta
    std::chrono::milliseconds averageDeltaMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(cumulativeDelta / (double)numBuffersCompleted);
    std::chrono::milliseconds maxDeltaMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(maxDelta);
    
    printf("TestStopped! Average Delta sec: %f  ms: %lld   Max Delta sec: %f  ms: %lld\n",
           cumulativeDelta.count() / (double)numBuffersCompleted, averageDeltaMilliseconds.count(),
           maxDelta.count(), maxDeltaMilliseconds.count());
   
    return true;
}

void AudiblizerTestHarness::WaitOnTestCompletion()
{
    audioQueueingThreadTerminated.Wait();
    
}

void AudiblizerTestHarness::BuffersCompleted(const BuffersCompletedVector &buffersCompleted)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return;
    }
    
    // if this is the first call, don't track anything
    if(!firstCallToBuffersCompleted)
    {
        firstCallToBuffersCompleted = true;
        lastCallToBuffersCompleted = std::chrono::high_resolution_clock::now();
        return;
    }
    
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> deltaFloatingPointSeconds = now - lastCallToBuffersCompleted;
    std::chrono::milliseconds deltaMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(deltaFloatingPointSeconds);
    
    OutputData outputData;
    outputData.numBuffers = buffersCompleted.size();
    outputData.deltaFloatingPointSeconds = deltaFloatingPointSeconds;
    outputData.deltaMilliseconds = deltaMilliseconds;
    
    outputDataQueueMutex.lock();
    outputDataQueue.push(outputData);
    outputDataQueueMutex.unlock();
    
    lastCallToBuffersCompleted = now;
    cumulativeDelta += deltaFloatingPointSeconds;
    numBuffersCompleted += buffersCompleted.size();
    
    if(deltaFloatingPointSeconds > maxDelta)
    {
        maxDelta = deltaFloatingPointSeconds;
    }
    
    return;
}

void AudiblizerTestHarness::AudioQueueingThreadProc(AudiblizerTestHarness *audiblizerTestHarness)
{
    uint32_t videoSegmentIter = 0;
    uint32_t videoSegmentFrameIter = 0;
    double   remainder = 0;
    
    while(true)
    {
        if(!audiblizerTestHarness->audioQueueingThreadRunning)
        {
            break;
        }
        
        // ensure that we are still in valid territory
        if(videoSegmentIter >= audiblizerTestHarness->videoSegments.size())
        {
            break;
        }
        
        // figure out max durations
        double queuedAudioDurationSeconds = audiblizerTestHarness->audiblizer->QueuedAudioDurationSeconds();
        double maxDurationToBeQueued = audiblizerTestHarness->maxQueuedAudioDurationSeconds - queuedAudioDurationSeconds;
        
        // if the audiblizer is close to being overloaded, sleep for a bit
        if(maxDurationToBeQueued <= 0.25)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }
        
        // queue as much audio as we are able to
        // --------------------------------------------------
        int32_t queueableAudioDurationMilliseconds = maxDurationToBeQueued * 1000.0;
        Audiblizer::AudioChunkVector audioChunks;
        
        while(queueableAudioDurationMilliseconds > 0)
        {
            if(videoSegmentFrameIter >= audiblizerTestHarness->videoSegments[videoSegmentIter].numVideoFrames)
            {
                videoSegmentFrameIter = 0;
                videoSegmentIter++;
            }
            
            if(videoSegmentIter >= audiblizerTestHarness->videoSegments.size())
            {
                break;
            }
            
            // derive info on video frames remaining in current video segment
            uint32_t numVideoFramesRemaining = audiblizerTestHarness->videoSegments[videoSegmentIter].numVideoFrames - videoSegmentFrameIter;
            uint32_t videoFrameDurationMilliseconds = (audiblizerTestHarness->videoSegments[videoSegmentIter].sampleDuration * 1000) / audiblizerTestHarness->videoSegments[videoSegmentIter].timeScale;
            uint32_t numVideoFramesRemainingDurationMilliseconds = numVideoFramesRemaining * videoFrameDurationMilliseconds;
            
            // derive info on the audio
            double   audioFramesPerVideoFrame = (audiblizerTestHarness->videoSegments[videoSegmentIter].sampleDuration / (double) audiblizerTestHarness->videoSegments[videoSegmentIter].timeScale) * audiblizerTestHarness->audioSampleRate;
            uint32_t audioFrameByteLength = Audiblizer::AudioFormatFrameByteLength(audiblizerTestHarness->audioFormat);
            
            // derive how much audio that we will here be queueing from the CURRENT video segment
            uint32_t currentChunkMilliseconds = queueableAudioDurationMilliseconds;
            if(currentChunkMilliseconds > numVideoFramesRemainingDurationMilliseconds)
            {
                currentChunkMilliseconds = numVideoFramesRemainingDurationMilliseconds;
            }
            
            // finally derive the number of video frames that we will be queueing from the CURRENT video segment
            uint32_t numVideoFramesToQueue = currentChunkMilliseconds / videoFrameDurationMilliseconds;
            
            // acutally create the audio chunks and place them into the the audioChunksVector
            for(uint32_t i = 0; i < numVideoFramesToQueue; i++)
            {
                Audiblizer::AudioChunk audioChunk;
                
                // see if we have to add any extra audio frames due to the remainder
                remainder += (audioFramesPerVideoFrame - (uint32_t)audioFramesPerVideoFrame);
                
                uint32_t remainderAdd = 0;
                if(remainder > 1.0)
                {
                    remainderAdd = 1;
                    remainder -= 1.0;
                }
                
                uint32_t totalAudioFrames = ((uint32_t)audioFramesPerVideoFrame) + remainderAdd;
                uint32_t totalAudioFramesByteLength = totalAudioFrames * audioFrameByteLength;
                
                // failsafe to not try to make a queue of audio that is longer that the
                // entire buffer of sample audio. As this should never happen in production,
                // and should never even happen here in this test WE DO NOT MESS AROUND
                // WITH remainder, WHICH WE SHOULD DO IF HITTING THIS CONDITION WERE TO
                // BE A REAL POSSIBILITY
                if(totalAudioFramesByteLength > audiblizerTestHarness->audioDataSize)
                {
                    totalAudioFrames = (uint32_t)(audiblizerTestHarness->audioDataSize / audioFrameByteLength);
                    totalAudioFramesByteLength = totalAudioFrames * audioFrameByteLength;
                }
                
                // if the current chunk would take us past the end of the sample audio, then reset the pointer
                size_t currentAudioByteLocation = audiblizerTestHarness->audioDataPtr - audiblizerTestHarness->audioData;
                if(currentAudioByteLocation + (totalAudioFrames * audioFrameByteLength) >= audiblizerTestHarness->audioDataSize)
                {
                    audiblizerTestHarness->audioDataPtr = audiblizerTestHarness->audioData;
                }
                
                // fill up the audio chunk
                audioChunk.buffer = audiblizerTestHarness->audioDataPtr;
                audioChunk.bufferSize = totalAudioFramesByteLength;
                audioChunk.format = audiblizerTestHarness->audioFormat;
                audioChunk.sampleRate = audiblizerTestHarness->audioSampleRate;
                
                // advance the audioDataPtr
                audiblizerTestHarness->audioDataPtr += totalAudioFramesByteLength;
                
                // push the chunk onto the audioChunks vector
                audioChunks.push_back(audioChunk);
            }
            
            // keep track of how much audio we just added to the audioChunks vector
            videoSegmentFrameIter += numVideoFramesToQueue;
        }
        
        // queue the (valid) audioChunk onto the audiblizer
        if(audioChunks.size() > 0)
        {
            audiblizerTestHarness->audiblizer->QueueAudio(audioChunks);
        }
    }
    
    // spin wait for audiblizer buffers to drain
    while(audiblizerTestHarness->audiblizer->NumBuffersQueued() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // tell topside that thread completed
    audiblizerTestHarness->audioQueueingThreadTerminated.Signal();
}

void AudiblizerTestHarness::DataOutputThreadProc(AudiblizerTestHarness *audiblizerTestHarness)
{
   bool queueIsEmpty = true;
    
    while(audiblizerTestHarness->dataOutputThreadRunning || !queueIsEmpty)
    {
        OutputData outputData;
        queueIsEmpty = true;
        
        audiblizerTestHarness->outputDataQueueMutex.lock();
        if(!audiblizerTestHarness->outputDataQueue.empty())
        {
            outputData = audiblizerTestHarness->outputDataQueue.front();
            audiblizerTestHarness->outputDataQueue.pop();
            queueIsEmpty = false;
        }
        audiblizerTestHarness->outputDataQueueMutex.unlock();
        
        if(!queueIsEmpty)
        {
            printf("AudioBuffersCompleted: %lu  Time sec:%f  ms:%lld\n", outputData.numBuffers, outputData.deltaFloatingPointSeconds.count(), outputData.deltaMilliseconds.count());
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    }
}

void* AudiblizerTestHarness::GenerateAudioSample(uint32_t sampleRate, double durationSeconds, bool stereo, size_t *bufferSizeOut)
{
    if(audioFormat != Audiblizer::AudioFormat_Stereo16)
    {
        return nullptr;
    }
    
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

void AudiblizerTestHarness::FreeAudioSample(void* data)
{
    if(data != nullptr)
    {
        free(data);
        data = nullptr;
    }
}
