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
    videoTimerPeriod(1001.0 / 30000.0),
    firstCallToPumpVideoFrame(false),
    audiblizer(nullptr),
    highPrecisionTimer(nullptr),
    maxDelta(std::chrono::duration<float>::zero()),
    minDelta(10000.0),
    cumulativeDelta(std::chrono::duration<float>::zero()),
    audioChunkIter(0),
    videoFrameIter(0),
    lastVideoFrameIter(0),
    videoTimerIter(0),
    avEqualizer(0),
    numPumpsCompleted(0),
    videoFrameHiccup(false),
    avDrift(false),
    audioQueueingThread(nullptr),
    audioQueueingThreadRunning(false),
    audioQueueingThreadTerminated(false, false),
    audioSampleRate(48000),
    audioIsStereo(true),
    audioIsSilence(true),
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
    
    // Audiblizer
    // --------------------------------------------
    audiblizer = std::make_shared<Audiblizer>();
    if(!audiblizer->Initialize())
    {
        audiblizer = nullptr;
        retVal = false;
        
        goto Exit;
    }
    
    audiblizer->SetBuffersCompletedListener(getptr());
    
    // HighPrecisionTimer
    // --------------------------------------------
    highPrecisionTimer = std::make_shared<HighPrecisionTimer>();
    
    // add 'this' and audiblizer as delegates to timer
    highPrecisionTimer->AddDelegate(getptr());
    highPrecisionTimer->AddDelegate(audiblizer);
    
    // Sample AudioData
    // --------------------------------------------
    FreeAudioSample(audioData);
    audioData = (uint8_t*)GenerateAudioSample(audioSampleRate, audioDurationSeconds, audioIsStereo, audioIsSilence, &audioDataSize);
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
    firstCallToPumpVideoFrame = false;
    maxDelta = std::chrono::duration<float>::zero();
    minDelta = std::chrono::duration<float>(10000.0);
    cumulativeDelta = std::chrono::duration<float>::zero();
    numPumpsCompleted = 0;
    audioChunkIter = 0;
    videoFrameIter = 0;
    lastVideoFrameIter = 0;
    videoTimerIter = 0;
    avEqualizer    = 0;
    videoFrameHiccup = false;
    avDrift = false;
    videoSegmentsTotalNumFrames = 0;
    
    audioDataPtr = audioData;
    videoTimerPeriod = !videoSegments.empty() ? (videoSegments[0].sampleDuration / (double) videoSegments[0].timeScale) : (1001.0 / 30000.0);
    
    // find the total num frames in the video segments
    for(uint32_t i = 0; i < videoSegments.size(); i++)
    {
        videoSegmentsTotalNumFrames += videoSegments[i].numVideoFrames;
    }
    
    // start up the high precision timer
    highPrecisionTimer->Start();
    
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
    
    // stop high precision timer
    // -------------------------------------
    highPrecisionTimer->Stop();
    highPrecisionTimer->RemoveAllDelegates();
    
    // stop the threads
    // -------------------------------------
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
    printf("***TestStopped***\n");
    printf("Average Delta sec:%f - Max Delta sec:%f VFI:%06llu - Min Delta sec:%f VFI:%06llu\n", cumulativeDelta.count() / (double)numPumpsCompleted, maxDelta.count(), maxDeltaVideoFrameIter, minDelta.count(), minDeltaVideoFrameIter);
    if(videoFrameHiccup)
    {
        printf("*** VIDEO FRAME HICCUPS OCCURRED!!! ***");
    }
    else
    {
        printf("No video frame hiccups occurred\n");
    }
    
    if(avDrift)
    {
        printf("*** AUDIO/VIDEO DRIFT OCCURRED!!! ***");
    }
    else
    {
        printf("No audio/video drift occurred\n");
    }
    
    return true;
}

void AudiblizerTestHarness::WaitOnTestCompletion()
{
    audioQueueingThreadTerminated.Wait();
    
}

void AudiblizerTestHarness::AudioChunkCompleted(const AudioChunkCompletedVector &audioChunksCompleted)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return;
    }
    
    audioChunkIter += (uint32_t)audioChunksCompleted.size();
    PumpVideoFrame(PumpVideoFrameSender_AudioUnqueuer, (int32_t)audioChunksCompleted.size());
    
    // NOTE: In a scheme where the audio chunk was both dynamically allocated and to be queued on
    //       the Audiblizer only once, we would here dispose of the audio memory here
    
    return;
}

void AudiblizerTestHarness::TimerPing()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if(!initialized)
    {
        return;
    }
    
    videoTimerIter++;
    PumpVideoFrame(PumpVideoFrameSender_VideoTimer);
}

void AudiblizerTestHarness::PumpVideoFrame(PumpVideoFrameSender sender, int32_t numPumps)
{
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> deltaFloatingPointSeconds = now - lastCallToPumpVideoFrame;
    std::chrono::duration<float> totalFloatingPointSeconds = now - playbackStart;
    OutputData outputData;
    
    switch(sender)
    {
        case PumpVideoFrameSender_VideoTimer:
        {
            avEqualizer += numPumps;
            
            if(avEqualizer > 0)
            {
                videoFrameIter += numPumps;
            }
            else
            {
                goto Exit;
            }
            
            break;
        }
        case PumpVideoFrameSender_AudioUnqueuer:
        {
            // audio dequeueing scenarios:
            //
            // 1) audio is being unqueued in single buffer units, and so it can (roughly) keep up w/ video
            //      a) video is running slightly faster than audio
            //
            //      b) audio is running slightly faster than video
            //
            //
            // 2) audio CANNOT be unqueued in single buffer units (as we see w/ Windows unqueueing audio buffers
            //    that are sized to match frames of 1001/60000 video), and so we must allow the video to run on
            //    ahead of the audio dequeueing, and then ***gracefully*** true up when audio can be dequeued
            // -----------------------------------------------------------------------------------------------
            
            avEqualizer -= numPumps;
            
            if(avEqualizer < 0)
            {
                // if audio is taking over the timing scheme, then consume all of the ticks that audio has entered
                // into the system and then reset the video clock so that video is the one that drives playback once more
                // (because the video timer is much smoother than the audio dequeueing scheme)
                videoFrameIter += abs(avEqualizer);
                avEqualizer = 0;
                RefreshLastPing(); // the HighPrecisionTimer Delegate base-class of *this* class represents Video!!!
            }
            else
            {
                goto Exit;
            }
        }
    }
    
    // *********************************************************
    // *********************************************************
    // We would do something here to pump the video renderer
    // to render either the NEXT frame, or SKIP a frame and then
    // render the frame after that!!!
    // *********************************************************
    // *********************************************************
    
    // Instead, here we just output the data
    // ---------------------------------------------------------
    
    // however, if this is the first call, don't output anything
    if(!firstCallToPumpVideoFrame)
    {
        firstCallToPumpVideoFrame = true;
        lastCallToPumpVideoFrame = std::chrono::high_resolution_clock::now();
        playbackStart = lastCallToPumpVideoFrame;
        return;
    }
    
    // if we are trying to pump erroneous frames (which can occur
    // near shutdown time
    if(videoFrameIter > videoSegmentsTotalNumFrames)
    {
        return;
    }
    
    now = std::chrono::high_resolution_clock::now();
    deltaFloatingPointSeconds = now - lastCallToPumpVideoFrame;
    totalFloatingPointSeconds = now - playbackStart;
    
    outputData.pumpVideoFrameSender = sender;
    outputData.avEqualizer = avEqualizer;
    outputData.audioChunkIter = audioChunkIter;
    outputData.videoFrameIter = videoFrameIter;
    outputData.deltaFloatingPointSeconds = deltaFloatingPointSeconds;
    outputData.totalFloatingPointSeconds = totalFloatingPointSeconds;
    
    outputDataQueueMutex.lock();
    outputDataQueue.push(outputData);
    outputDataQueueMutex.unlock();
    
    lastCallToPumpVideoFrame = now;
    cumulativeDelta += deltaFloatingPointSeconds;
    numPumpsCompleted += numPumps;
    
    if(deltaFloatingPointSeconds > maxDelta)
    {
        maxDelta = deltaFloatingPointSeconds;
        maxDeltaVideoFrameIter = videoFrameIter;
    }
    
    if(deltaFloatingPointSeconds < minDelta)
    {
        minDelta = deltaFloatingPointSeconds;
        minDeltaVideoFrameIter = videoFrameIter;
    }
    
Exit:
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
            
            // remove the amount that we just queued
            queueableAudioDurationMilliseconds -= currentChunkMilliseconds;
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
    bool vfHiccup = false;
    
    while(audiblizerTestHarness->dataOutputThreadRunning || !queueIsEmpty)
    {
        OutputData outputData;
        queueIsEmpty = true;
        vfHiccup = false;
        
        audiblizerTestHarness->outputDataQueueMutex.lock();
        if(!audiblizerTestHarness->outputDataQueue.empty())
        {
            outputData = audiblizerTestHarness->outputDataQueue.front();
            audiblizerTestHarness->outputDataQueue.pop();
            queueIsEmpty = false;
        }
        audiblizerTestHarness->outputDataQueueMutex.unlock();
        
        if(!queueIsEmpty && outputData.videoFrameIter <= audiblizerTestHarness->videoSegmentsTotalNumFrames)
        {
            // handle info regarding last VFI
            // ---------------------------------------------------------------
            if(audiblizerTestHarness->lastVideoFrameIter != 0)
            {
                if(audiblizerTestHarness->lastVideoFrameIter + 1 != outputData.videoFrameIter)
                {
                    audiblizerTestHarness->videoFrameHiccup = vfHiccup = true;
                }
            }
            
            audiblizerTestHarness->lastVideoFrameIter = outputData.videoFrameIter;
        
            // see if there was any av drift
            // ---------------------------------------------------------------
            if(abs(outputData.audioChunkIter - outputData.videoFrameIter) > 1)
            {
                audiblizerTestHarness->avDrift = true;
                
                printf("*** DRIFT ***  ");
            }
            
            printf("Sender:%s   A/V Eq:%04lld   ACI:%06lld   VFI:%06lld%s  delta sec:%f   total sec:%f\n",
                   outputData.pumpVideoFrameSender == PumpVideoFrameSender_VideoTimer ? "V" : "A",
                   outputData.avEqualizer,
                   outputData.audioChunkIter, outputData.videoFrameIter,
                   vfHiccup ? "*" : " ",
                   outputData.deltaFloatingPointSeconds.count(),
                   outputData.totalFloatingPointSeconds.count());
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    }
}

void* AudiblizerTestHarness::GenerateAudioSample(uint32_t sampleRate, double durationSeconds, bool stereo, bool silence, size_t *bufferSizeOut)
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
    
    if(!silence)
    {
        for (uint32_t i = 0; i < bufferChannelFrames * numChannels; i += 2)
        {
            buffer[i] = 32768 - ((i % 100) * 660);
            buffer[i+1] = buffer[i];
        }
    }
    else
    {
        memset(buffer, 0, bufferSize);
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
