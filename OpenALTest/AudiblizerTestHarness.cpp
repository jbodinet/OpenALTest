// ****************************************************************************
// MIT License
//
// Copyright (c) 2019 Joshua E Bodinet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ****************************************************************************

#include "AudiblizerTestHarness.h"
#include <cmath>

const Audiblizer::AudioFormat AudiblizerTestHarness::audioFormat = Audiblizer::AudioFormat_Stereo16;

AudiblizerTestHarness::AudiblizerTestHarness() :
    audioData(nullptr),
    audioDataPtr(nullptr),
    audioDataSize(0),
    audioDataTotalNumDatums(0),
    audioDataTotalNumFrames(0),
    firstCallToPumpVideoFrame(false),
    audiblizer(nullptr),
    highPrecisionTimer(nullptr),
    audioChunkIter(0),
    videoFrameIter(0),
    lastVideoFrameIter(0),
    videoTimerIter(0),
    avEqualizer(0),
    audioRunningSlowAccum(0),
    videoFrameHiccup(false),
    maxVideoFrameHiccup(0),
    avDrift(false),
    avDriftNumFrames(0),
    maxAVDrift(0),
    audioQueueingThread(nullptr),
    audioQueueingThreadRunning(false),
    audioQueueingThreadTerminated(false, false),
    audioSampleRate(0),
    audioIsStereo(true),
    audioIsSilence(true),
    audioDurationSeconds(0.0),
    audioPlayrateFactor(1.0),
    adversarialTestingAudioPlayrateFactor(1.0),
    adversarialTestingAudioChunkCacheSize(1),
    adversarialTestingAudioChunkCacheAccum(0),
    maxQueuedAudioDurationSeconds(4.0),
    dataOutputThread(nullptr),
    dataOutputThreadRunning(false),
    dataOutputter(nullptr),
    initialized(false)
{
    

}

AudiblizerTestHarness::~AudiblizerTestHarness()
{
    StopTest();
    FreeAudioSample(audioData);
    audioData = nullptr;
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
    if(audiblizer == nullptr || !audiblizer->Initialize())
    {
        audiblizer = nullptr;
        retVal = false;
        
        goto Exit;
    }
    
    audiblizer->SetBuffersCompletedListener(getptr());
    
    // VideoTimerDelegate
    // --------------------------------------------
    videoTimerDelegate = std::make_shared<VideoTimerDelegate>();
    if(videoTimerDelegate == nullptr)
    {
        retVal = false;
        
        goto Exit;
    }
    
    videoTimerDelegate->SetTimerPingListener(getptr());
    
    // HighPrecisionTimer
    // --------------------------------------------
    highPrecisionTimer = std::make_shared<HighPrecisionTimer>();
    
    // add videoTimerDelegate and audiblizer as delegates to timer
    highPrecisionTimer->AddDelegate(videoTimerDelegate);
    highPrecisionTimer->AddDelegate(audiblizer);
    
    initialized = true;
    
Exit:
    return retVal;
}

bool AudiblizerTestHarness::LoadAudio(const char *filePath, uint32_t sampleRate)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    bool success = true;
    
    if(!initialized || filePath == nullptr)
    {
        return false;
    }
    
    // Sample AudioData
    // --------------------------------------------
    
    // ditch any existing audio data
    FreeAudioSample(audioData);
    audioData = nullptr;
    audioDataPtr = nullptr;
    audioDataSize = 0;
    audioDataTotalNumDatums = 0;
    audioDataTotalNumFrames = 0;
    audioSampleRate = 0;
    audioIsStereo = false;
    audioIsSilence = false;
    audioDurationSeconds = 0.0;
    
    // attempt to load audio
    success = Load16bitStereoPCMAudioFromFile(filePath, sampleRate);
    if(!success)
    {
        FreeAudioSample(audioData);
        audioData = nullptr;
        audioDataPtr = nullptr;
        audioDataSize = 0;
        audioDataTotalNumDatums = 0;
        audioDataTotalNumFrames = 0;
        audioSampleRate = 0;
        audioIsStereo = false;
        audioIsSilence = false;
        audioDurationSeconds = 0.0;
        
        goto Exit;
    }
    
Exit:
    return success;
}

bool AudiblizerTestHarness::GenerateSampleAudio(uint32_t sampleRate, bool stereo, bool silence, double durationSeconds)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    bool success = true;
    
    if(!initialized)
    {
        return false;
    }
    
    // Sample AudioData
    // --------------------------------------------
    
    // ditch any existing audio data
    FreeAudioSample(audioData);
    audioData = nullptr;
    audioDataPtr = nullptr;
    audioDataSize = 0;
    audioDataTotalNumDatums = 0;
    audioDataTotalNumFrames = 0;
    audioSampleRate = 0;
    audioIsStereo = false;
    audioIsSilence = false;
    audioDurationSeconds = 0.0;
    
    // generate audio sample
    audioData = (uint8_t*)GenerateAudioSample(sampleRate, durationSeconds, stereo, silence, &audioDataSize);
    if(audioData == nullptr)
    {
        success = false;
        goto Exit;
    }
    
    audioDataPtr = audioData;
    audioSampleRate = sampleRate;
    audioIsStereo = stereo;
    audioIsSilence = silence;
    audioDurationSeconds = durationSeconds;
    audioDataTotalNumDatums = audioDataSize / sizeof(uint16_t);
    audioDataTotalNumFrames = audioDataSize / Audiblizer::AudioFormatFrameByteLength(audioFormat);
    
Exit:
    return success;
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
    videoTimerDelegate->PrepareForDestruction();
    audiblizer = nullptr;
    videoTimerDelegate = nullptr;
    initialized = false;
}

bool AudiblizerTestHarness::StartTest(const VideoSegments &videoSegmentsArg, double adversarialTestingAudioPlayrateFactorArg, uint32_t adversarialTestingAudioChunkCacheSizeArg, uint32_t numAdversarialPressureTheads)
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
    
    if(videoSegmentsArg.empty())
    {
        return false;
    }
    
    videoPlaymap.clear();
    videoSegmentOutputData.clear();
    videoSegments = videoSegmentsArg;
    adversarialTestingAudioPlayrateFactor = adversarialTestingAudioPlayrateFactorArg > 0 ? adversarialTestingAudioPlayrateFactorArg : -adversarialTestingAudioPlayrateFactorArg;
    adversarialTestingAudioChunkCacheSize = adversarialTestingAudioChunkCacheSizeArg;
    adversarialTestingAudioChunkCacheAccum = 0;
    firstCallToPumpVideoFrame = false;
    audioPlaybackDurationActual = std::chrono::duration<double>::zero();
    audioPlaybackDurationIdeal = 0;
    firstCallToAudioChunkCompleted = false;
    audioDataPtr = audioData;
    audioChunkIter = 0;
    videoFrameIter = 0;
    lastVideoFrameIter = 0;
    videoTimerIter = 0;
    avEqualizer    = 0;
    audioRunningSlowAccum = 0;
    videoFrameHiccup = false;
    maxVideoFrameHiccup = 0;
    avDrift = false;
    avDriftNumFrames = 0;
    maxAVDrift = 0;
    videoSegmentsTotalNumFrames = 0;
    frameRateAdjustedOnFrameIndex = 0;
    videoSegmentOutputDataIter = 0;
   
    // parse the video segments
    for(uint32_t i = 0; i < videoSegments.size(); i++)
    {
        // generate a video playmap using the video segments
        // -------------------------------------------------------
        
        // insert the segment with the ***current value*** of videoSegmentsTotalNumFrames,
        // which is the starting video frame-index of this segment (as we use the value
        // ***before*** we add to it the numFrames for *this* segment)
        videoPlaymap.insert(VideoPlaymapPair(videoSegmentsTotalNumFrames, videoSegments[i]));
        
        // generate the total num frames in all the video segments
        videoSegmentsTotalNumFrames += videoSegments[i].numVideoFrames;
        
        // prepare a VideoSegmentsOutputData element for each segment
        videoSegmentOutputData.push_back(VideoSegmentOutputData());
    }
    
    // start the video timer off using the timing values for the first segment of video, also resetting the audioPlayrateFactor
    videoTimerDelegate->SetTimerPeriod(videoPlaymap.begin()->second.sampleDuration / (double) videoPlaymap.begin()->second.timeScale);
    videoTimerDelegate->SetAudioPlayrateFactor(1.0);
    
    // underscore that we used the frame rate of the first video segment
    frameRateAdjustedOnFrameIndex = videoPlaymap.begin()->first;
    
    // underscore that we are on the first video segment in the VideoSegmentsOutputData
    videoSegmentOutputDataIter = 0;
    
    // start up the adversarial pressure threads (if there are any...)
    if(numAdversarialPressureTheads != 0)
    {
        for(uint32_t i = 0; i < numAdversarialPressureTheads; i++)
        {
            std::shared_ptr<AdversarialPressureThread> adversarialPressureThread = AdversarialPressureThread::CreateShared();
            adversarialPressureThreads.push_back(adversarialPressureThread);
        }
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
    
    // stop the adversarial pressure threads
    // -------------------------------------
    if(!adversarialPressureThreads.empty())
    {
        for(uint32_t i = 0; i < adversarialPressureThreads.size(); i++)
        {
            adversarialPressureThreads[i]->Kill();
        }
        
        adversarialPressureThreads.clear();
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
    
    if(!firstCallToAudioChunkCompleted)
    {
        lastCallToAudioChunkCompleted = std::chrono::high_resolution_clock::now();
        firstCallToAudioChunkCompleted = true;
    }
    else
    {
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        audioPlaybackDurationActual += (now - lastCallToAudioChunkCompleted);
        
        for(uint32_t i = 0; i < audioChunksCompleted.size(); i++)
        {
            audioPlaybackDurationIdeal += audioChunksCompleted[i].duration;
        }
        
        lastCallToAudioChunkCompleted = now;
    }
    
    adversarialTestingAudioChunkCacheAccum += (int32_t)audioChunksCompleted.size();
    
    // NOTE: adversarialTestingAudioChunkCacheSize is used so that we are able to test
    //       the case where the client implementation of OpenAL is only able to return to
    //       us, say, 2, 3, or 4, chunks of audio at a time. In an actual implementation
    //       we would always call PumpVideoFrame() every time this AudioChunkCompleted() is
    //       called, and we would pumping the video frame iter a total number of times
    //       equal to the number of chunks of audio that are contained with 'audioChunksCompleted',
    //       which is the number of chunks of audio that were just dequeue via OpenAL
    if(adversarialTestingAudioChunkCacheAccum >= adversarialTestingAudioChunkCacheSize)
    {
        PumpVideoFrame(PumpVideoFrameSender_AudioUnqueuer, adversarialTestingAudioChunkCacheAccum);
        audioChunkIter += adversarialTestingAudioChunkCacheAccum;
        adversarialTestingAudioChunkCacheAccum = 0;
    }
    
    // NOTE: In a scheme where the audio chunk was both dynamically allocated and to be queued on
    //       the Audiblizer only once, we would here dispose of the audio memory here
    
    return;
}

void AudiblizerTestHarness::VideoTimerPing()
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
    uint64_t numActionablePumps = numPumps; // num pumps that we are actually going to act upon within this call
    OutputData outputData;
    bool adjustedFramerate = false;
    
    switch(sender)
    {
        case PumpVideoFrameSender_VideoTimer:
        {
            avEqualizer += numPumps;
            
            if(avEqualizer > 0)
            {
                numActionablePumps = numPumps; // we act on the full number of pumps
                videoFrameIter += numActionablePumps;
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
            //    ahead of the audio dequeueing, and then ***gracefully*** true up when audio can be dequeued.
            //    However, additionally, once we dequeue the multi-chunks of audio, we can still find out that:
            //      a) video is running slightly faster than audio
            //
            //      b) audio is running slightly faster than video
            // -----------------------------------------------------------------------------------------------
            
            avEqualizer -= numPumps;
            
            // if avEqualizer < 0, then audio has taken over the timing scheme, which we do NOT want.
            // NOTE: audio can take over the timing scheme in one of two ways:
            //          a) audio can be playing back at roughly the expected rate, yet it is playing slight faster
            //             than video
            //          b) audio is erroneously playing back at a rate *faster* than it should, which still satisfies
            //             the case here--that audio is playing back faster than video
            //       Both of the above cases are handled by this 'avEqualizer < 0' clause
            // -----------------------------------------------------------------------------------------------
            if(avEqualizer < 0)
            {
                // if audio is taking over the timing scheme, then consume all of the ticks that audio has entered
                // into the system and then reset the video clock so that video is the one that drives playback once more
                // (because the video timer is much smoother than the audio dequeueing scheme)
                // -----------------------------------------------------------------------------------------------
                numActionablePumps = abs(avEqualizer); // we only act on the remainder of pumps
                videoFrameIter += numActionablePumps;
                avEqualizer = 0;
                videoTimerDelegate->RefreshLastPing();
                
                // reset the accum that tracks audio running slower than video
                audioRunningSlowAccum = 0;
            }
            // if it is still the case that avEqualizer > 0, then we here assume
            // that audio is running slighty *slower* than the video. We assume this as,
            // even if the audiblizer dequeues in multiple chunks, an audio dequeue should return
            // avEqualizer to '0'
            else if(avEqualizer > 0)
            {
                // note that we detected the audio running slowly
                audioRunningSlowAccum++;
                
                // as the audio card can exhibit localized-wonkiness and yet still be overall
                // performant in keeping up with the dequeue-ability of spent audio buffers,
                // we add a 'audioRunningSlowAccum' accumulater, which only triggers a resetting
                // of the video playback timer given the attainment of a certain threshold
                if(audioRunningSlowAccum > audioRunningSlowThreshold)
                {
                    // ************************************************************************
                    // ************************************************************************
                    // ************************************************************************
                    // IMPORTANT QUESTION!!!
                    //
                    // Do we also want to check (and reset) the video clock in the event
                    // that the audio is found to be running FAST? If we only check and reset
                    // the clock when the audio is running slow, then the following code, which
                    // resets the video timer using a slightly slower speed, has no counter-acting
                    // force that will speed up the video timer given audio that starts out running
                    // slow, but then picks up speed during playback
                    // ************************************************************************
                    // ************************************************************************
                    // ************************************************************************
                    
                    // we alter the audio playrate factor to represent what is going on with audio
                    audioPlayrateFactor = audioPlaybackDurationActual.count() / audioPlaybackDurationIdeal;
                    
                    // HOWEVER!!! if we are adversarially (and, thus, artificially) testing the handling of
                    // improperly-playing audio, then the audioPlayrate should still be calculated to be 1.0, above
                    // (as we are actually playing the audio at an expected rate, just not what is expected
                    // as compared to the video frame rate -- thus it is adversarial). In this case we need to
                    // set audioPlayrateFactor per how we are futzing with the audio
                    if(adversarialTestingAudioPlayrateFactor != 1.0)
                    {
                        audioPlayrateFactor = adversarialTestingAudioPlayrateFactor;
                    }
                    
                    // update the audioPlayrateFactor in the Video Timer and refresh the timer ping
                    videoTimerDelegate->SetAudioPlayrateFactor(audioPlayrateFactor);
                    videoTimerDelegate->RefreshLastPing();
                    
                    // reset the accum that tracks audio running slower than video
                    audioRunningSlowAccum = 0;
                }
                
                goto Exit;
            }
            else
            {
                // reset the accum that tracks audio running slower than video
                audioRunningSlowAccum = 0;
                
                goto Exit;
            }
        }
    }
    
    // If playback is multiframerate, then adjust the video timer period as necessary
    if(videoPlaymap.size() > 1)
    {
        // find the segment for the NEXT videoFrameIter
        VideoPlaymapIterator iter = videoPlaymap.lower_bound(videoFrameIter);
        if(iter == videoPlaymap.end())
        {
            iter--;
        }
        
        // advance to the next segment
        // Note: lower_bound() does not do exactly what we want, nor does upper_bound.
        //       We want an iterator to the value that meets the following criterias:
        //          1) the returned iterator will point to the element whose key
        //             is equal to or less than 'videoFrameIter'
        //          AND
        //          2) 'videoFrameIter' is strictly less than the key of the
        //             element in the map that follows the chosen element
        //
        // Thus: if keys are {0, 900}, and we are given a value of 0, we return the first
        //       element. If we are given 1, we return the first element. If we are given
        //       900, we return the second element. If we are given 901, we return the
        //       second element.
        //
        // The closest thing is to use lower_bound() and then decrement the iterator
        // if its key is greater than the value that we used in the lower_bound() search
        if(iter->first > videoFrameIter)
        {
            iter--;
        }
        
        // if we should perform a new adjustment
        if(frameRateAdjustedOnFrameIndex != iter->first)
        {
            // update the video timer period
            videoTimerDelegate->SetTimerPeriod(iter->second.sampleDuration / (double) iter->second.timeScale);
            
            // keep track of on which video frame the adjustment took place
            frameRateAdjustedOnFrameIndex = iter->first;
            
            // note that we adjusted frame rate
            adjustedFramerate = true;
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
    outputData.audioChunkIter = audioChunkIter + 1; // IMPORTANT NOTE: 'audioChunkIter' represents the audio chunk ***THAT WAS JUST DEQUEUED***!!!
                                                    //                  THUS THE ***CURRENT AUDIO CHUCK BEING PLAYED*** IS 'audioChunkIter + 1'.
                                                    //                  As 'videoFrameIter' represents the current video frame being played, we
                                                    //                  want 'audioChunkIter' to represent the current audio chunk being played,
                                                    //                  and specifically ***NOT*** the audio chunk that was just dequeued.
    outputData.adversarialTestingAudioChunkCacheAccum = adversarialTestingAudioChunkCacheAccum;
    outputData.videoFrameIter = videoFrameIter;
    outputData.deltaFloatingPointSeconds = deltaFloatingPointSeconds;
    outputData.totalFloatingPointSeconds = totalFloatingPointSeconds;
    
    outputDataQueueMutex.lock();
    outputDataQueue.push(outputData);
    outputDataQueueMutex.unlock();
    
    lastCallToPumpVideoFrame = now;
    videoSegmentOutputData[videoSegmentOutputDataIter].cumulativeDelta += deltaFloatingPointSeconds;
    videoSegmentOutputData[videoSegmentOutputDataIter].numPumpsCompleted += numActionablePumps;
    
    // figure out max / min deltas
    // HOWEVER! do not report max / min deltas for first or last frames
    // -----------------------------------------------------------------
    if(videoFrameIter != 0 &&
       videoFrameIter != 1 &&
       videoFrameIter != videoSegmentsTotalNumFrames)
    {
        if(deltaFloatingPointSeconds > videoSegmentOutputData[videoSegmentOutputDataIter].maxDelta)
        {
            videoSegmentOutputData[videoSegmentOutputDataIter].maxDelta = deltaFloatingPointSeconds;
            videoSegmentOutputData[videoSegmentOutputDataIter].maxDeltaVideoFrameIter = videoFrameIter;
        }
        
        if(deltaFloatingPointSeconds < videoSegmentOutputData[videoSegmentOutputDataIter].minDelta)
        {
            videoSegmentOutputData[videoSegmentOutputDataIter].minDelta = deltaFloatingPointSeconds;
            videoSegmentOutputData[videoSegmentOutputDataIter].minDeltaVideoFrameIter = videoFrameIter;
        }
    }
    
    // keep track of the rolling timer period (but only if we did NOT
    // adjust frame rate, as if we adjusted frame rate, then we ***already***
    // updated the timer to represent the frame rate, and so we would
    // erroneously be setting the timer here
    // -----------------------------------------------------------------
    if(!adjustedFramerate)
    {
        videoSegmentOutputData[videoSegmentOutputDataIter].timerPeriod = videoTimerDelegate->TimerPeriod();
    }
    
    // ***AFTER*** we have input all of the data for the current frame, if we found that we altered
    // the frame rate on this call, tick the iter for the VideoSegmentOutputData
    if(adjustedFramerate)
    {
        videoSegmentOutputDataIter++;
    }
    
Exit:
    return;
}

void AudiblizerTestHarness::AudioQueueingThreadProc(AudiblizerTestHarness *audiblizerTestHarness)
{
    uint32_t videoSegmentIter = 0;
    uint32_t videoSegmentFrameIter = 0;
    double   remainder = 0;
    
    std::string outputDataString;
    const uint32_t outputDataCStringSize = 512;
    char outputDataCString [outputDataCStringSize];
    
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
            
            // for *** test purposes only *** we allow for the value of audioFramesPerVideoFrame to
            // be scaled by 'audioPlayrateFactor', which allows us to mimic a system that plays
            // audio either too fast or too slow as compared to the explicit audio sample rate
            audioFramesPerVideoFrame *= audiblizerTestHarness->adversarialTestingAudioPlayrateFactor;
            
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
    // ------------------------------------------------------------
    while(audiblizerTestHarness->audiblizer->NumBuffersQueued() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // as audiblizer drives the heart beat, we can output end-of-test data here
    // -------------------------------------
    outputDataString += "*** TestStopped ***\n";
    
    if(audiblizerTestHarness->adversarialTestingAudioPlayrateFactor != 1.0)
    {
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "Adversarial AudioPlayrateFactor:%f\n", audiblizerTestHarness->adversarialTestingAudioPlayrateFactor);
        outputDataString += outputDataCString;
    }
    
    if(audiblizerTestHarness->audioPlayrateFactor != 1.0)
    {
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "Actual AudioPlayrateFactor:%f\n", audiblizerTestHarness->audioPlayrateFactor);
        outputDataString += outputDataCString;
    }
    
    if(audiblizerTestHarness->adversarialTestingAudioChunkCacheSize != 1)
    {
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "Adversarial AudioChunkCacheSize:%d\n", audiblizerTestHarness->adversarialTestingAudioChunkCacheSize);
        outputDataString += outputDataCString;
    }
    
    if(audiblizerTestHarness->adversarialPressureThreads.size() != 0)
    {
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "Adversarial PressureThreads count:%zu\n", audiblizerTestHarness->adversarialPressureThreads.size());
        outputDataString += outputDataCString;
    }
    
    if(audiblizerTestHarness->videoSegmentOutputDataIter == 0)
    {
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "VideoTimerPeriod:%f\n", audiblizerTestHarness->videoSegmentOutputData[0].timerPeriod);
        outputDataString += outputDataCString;
        
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "Average Delta sec:%f - Max Delta sec:%f VFI:%06llu - Min Delta sec:%f VFI:%06llu\n", audiblizerTestHarness->videoSegmentOutputData[0].cumulativeDelta.count() / (double)audiblizerTestHarness->videoSegmentOutputData[0].numPumpsCompleted, audiblizerTestHarness->videoSegmentOutputData[0].maxDelta.count(), audiblizerTestHarness->videoSegmentOutputData[0].maxDeltaVideoFrameIter, audiblizerTestHarness->videoSegmentOutputData[0].minDelta.count(), audiblizerTestHarness->videoSegmentOutputData[0].minDeltaVideoFrameIter);
        outputDataString += outputDataCString;
    }
    else
    {
        for(uint32_t i = 0; i <= audiblizerTestHarness->videoSegmentOutputDataIter; i++)
        {
            memset(outputDataCString, 0, outputDataCStringSize);
            sprintf(outputDataCString, "VideoSegment:%d  VideoTimerPeriod:%f\n", i, audiblizerTestHarness->videoSegmentOutputData[i].timerPeriod);
            outputDataString += outputDataCString;
            
            memset(outputDataCString, 0, outputDataCStringSize);
            sprintf(outputDataCString, "VideoSegment:%d  Average Delta sec:%f - Max Delta sec:%f VFI:%06llu - Min Delta sec:%f VFI:%06llu\n", i, audiblizerTestHarness->videoSegmentOutputData[i].cumulativeDelta.count() / (double)audiblizerTestHarness->videoSegmentOutputData[i].numPumpsCompleted, audiblizerTestHarness->videoSegmentOutputData[i].maxDelta.count(), audiblizerTestHarness->videoSegmentOutputData[i].maxDeltaVideoFrameIter, audiblizerTestHarness->videoSegmentOutputData[i].minDelta.count(), audiblizerTestHarness->videoSegmentOutputData[i].minDeltaVideoFrameIter);
            outputDataString += outputDataCString;
        }
    }
    
    if(audiblizerTestHarness->videoFrameHiccup)
    {
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "*** VIDEO FRAME HICCUPS OCCURRED!!! MAX HICCUP: %d VIDEO FRAMES ***", audiblizerTestHarness->maxVideoFrameHiccup);
        outputDataString += outputDataCString;
    }
    else
    {
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "No video frame hiccups occurred\n");
        outputDataString += outputDataCString;
    }
    
    if(audiblizerTestHarness->avDrift)
    {
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "*** AUDIO/VIDEO DRIFT OCCURRED!!! MAX DRIFT: %d VIDEO FRAMES - NUM FRAMES WITH DRIFT: %d - %% FRAMES WITH DRIFT: %f%% ***\n", audiblizerTestHarness->maxAVDrift, audiblizerTestHarness->avDriftNumFrames, (audiblizerTestHarness->avDriftNumFrames / (double) audiblizerTestHarness->videoSegmentsTotalNumFrames) * 100.0);
        outputDataString += outputDataCString;
    }
    else
    {
        memset(outputDataCString, 0, outputDataCStringSize);
        sprintf(outputDataCString, "No audio/video drift occurred\n");
        outputDataString += outputDataCString;
    }
    
    std::lock_guard<std::mutex> dataOutputterLock(audiblizerTestHarness->dataOutputterMutex);
    if(audiblizerTestHarness->dataOutputter != nullptr)
    {
        audiblizerTestHarness->dataOutputter->OutputData(outputDataString.c_str());
    }
    else
    {
        printf("%s", outputDataString.c_str());
    }
    
    // tell topside that thread completed
    // ------------------------------------------------------------
    audiblizerTestHarness->audioQueueingThreadTerminated.Signal();
}

void AudiblizerTestHarness::DataOutputThreadProc(AudiblizerTestHarness *audiblizerTestHarness)
{
    bool queueIsEmpty = true;
    bool vfHiccup = false;
    
    while(audiblizerTestHarness->dataOutputThreadRunning || !queueIsEmpty)
    {
        OutputData outputData;
        std::string outputDataString;
        const uint32_t outputDataCStringSize = 512;
        char outputDataCString [outputDataCStringSize];
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
            bool drift = false;
            
            // handle info regarding last VFI
            // ---------------------------------------------------------------
            if(audiblizerTestHarness->lastVideoFrameIter != 0)
            {
                if(audiblizerTestHarness->lastVideoFrameIter + 1 != outputData.videoFrameIter)
                {
                    audiblizerTestHarness->videoFrameHiccup = vfHiccup = true;
                    if(outputData.videoFrameIter - audiblizerTestHarness->lastVideoFrameIter > audiblizerTestHarness->maxVideoFrameHiccup)
                    {
                        audiblizerTestHarness->maxVideoFrameHiccup = (uint32_t) (outputData.videoFrameIter - audiblizerTestHarness->lastVideoFrameIter);
                    }
                }
            }
            
            audiblizerTestHarness->lastVideoFrameIter = outputData.videoFrameIter;
        
            // see if there was any av drift
            // NOTE: to keep things clean and sane, we add 'adversarialTestingAudioChunkCacheAccum'
            //       to the mix, so that when testing w/ cached audio pumps we do not erroneously
            //       report drift
            // ---------------------------------------------------------------
            if(abs((outputData.audioChunkIter + outputData.adversarialTestingAudioChunkCacheAccum) - outputData.videoFrameIter) > 1)
            {
                audiblizerTestHarness->avDrift = true;
                audiblizerTestHarness->avDriftNumFrames++;
                
                if(abs(outputData.audioChunkIter - outputData.videoFrameIter) > audiblizerTestHarness->maxAVDrift)
                {
                    audiblizerTestHarness->maxAVDrift = (uint32_t) abs(outputData.audioChunkIter - outputData.videoFrameIter);
                }
                
                drift = true;
            }
            
            if(audiblizerTestHarness->adversarialTestingAudioChunkCacheSize == 1)
            {
                memset(outputDataCString, 0, outputDataCStringSize);
                sprintf(outputDataCString,
                        "Sender:%s   A/V Eq:%04lld   ACI:%06lld   VFI:%06lld%s  delta sec:%f   total sec:%f",
                        outputData.pumpVideoFrameSender == PumpVideoFrameSender_VideoTimer ? "V" : "A",
                        outputData.avEqualizer,
                        outputData.audioChunkIter,
                        outputData.videoFrameIter,
                        vfHiccup ? "*" : " ",
                        outputData.deltaFloatingPointSeconds.count(),
                        outputData.totalFloatingPointSeconds.count());
                
                outputDataString += outputDataCString;
            }
            else
            {
                memset(outputDataCString, 0, outputDataCStringSize);
                sprintf(outputDataCString,
                        "Sender:%s   A/V Eq:%04lld   ACI:%06lld+%02d   VFI:%06lld%s  delta sec:%f   total sec:%f",
                        outputData.pumpVideoFrameSender == PumpVideoFrameSender_VideoTimer ? "V" : "A",
                        outputData.avEqualizer,
                        outputData.audioChunkIter,
                        outputData.adversarialTestingAudioChunkCacheAccum,
                        outputData.videoFrameIter,
                        vfHiccup ? "*" : " ",
                        outputData.deltaFloatingPointSeconds.count(),
                        outputData.totalFloatingPointSeconds.count());
                
                outputDataString += outputDataCString;
            }
            
            if(drift)
            {
                outputDataString += "   *** DRIFT ***\n";
            }
            else
            {
                outputDataString += "\n";
            }
            
            std::lock_guard<std::mutex> lock(audiblizerTestHarness->dataOutputterMutex);
            if(audiblizerTestHarness->dataOutputter != nullptr)
            {
                audiblizerTestHarness->dataOutputter->OutputData(outputDataString.c_str());
            }
            else
            {
                printf("%s", outputDataString.c_str());
            }
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

AudiblizerTestHarness::AdversarialPressureThread::AdversarialPressureThread() :
    thread(nullptr),
    threadRunning(false)
{
    
}

std::shared_ptr<AudiblizerTestHarness::AdversarialPressureThread> AudiblizerTestHarness::AdversarialPressureThread::CreateShared()
{
    std::shared_ptr<AdversarialPressureThread> adversarialPressureThread = std::make_shared<AdversarialPressureThread>();
    if(adversarialPressureThread == nullptr)
    {
        goto Exit;
    }
    
    if(!adversarialPressureThread->Start())
    {
        adversarialPressureThread = nullptr;
        goto Exit;
    }
    
Exit:
    return adversarialPressureThread;
}

bool AudiblizerTestHarness::AdversarialPressureThread::Start()
{
    std::lock_guard<std::mutex> lock(threadMutex);
    
    if(thread != nullptr)
    {
        return false;
    }
    
    threadRunning = true;
    thread = new (std::nothrow) std::thread(AdversarialPressureThreadProc, this);
    if(thread == nullptr)
    {
        threadRunning = false;
        return false;
    }
    
    return true;
}

void AudiblizerTestHarness::AdversarialPressureThread::Kill()
{
    std::lock_guard<std::mutex> lock(threadMutex);
    
    if(thread == nullptr)
    {
        return;
    }
    
    threadRunning = false;
    thread->join();
    
    delete thread;
    thread = nullptr;
}

void AudiblizerTestHarness::AdversarialPressureThread::AdversarialPressureThreadProc(AdversarialPressureThread *adversarialPressureThread)
{
    double value = 1.0;
    
    while(adversarialPressureThread->threadRunning)
    {
        value = ((((value + value) * value) - value) / value);
        value = value > 0 ? value : -value;
        value = sqrt(value);
        value = ((uint64_t)value) % 100;
    }
}
