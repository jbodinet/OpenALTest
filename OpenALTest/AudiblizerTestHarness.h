//
//  AudiblizerTestHarness.h
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/5/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#ifndef AudiblizerTestHarness_h
#define AudiblizerTestHarness_h

#include "HighPrecisionTimer.h"
#include "Audiblizer.h"
#include "VideoTimerDelegate.h"
#include "Event.h"

#include <vector>
#include <queue>
#include <thread>
#include <memory>
#include <chrono>
#include <mutex>

class AudiblizerTestHarness : public Audiblizer::AudioChunkCompletionListener, public VideoTimerDelegate::TimerPingListener, public std::enable_shared_from_this<AudiblizerTestHarness>
{
public:
    AudiblizerTestHarness();
    ~AudiblizerTestHarness();
    
    std::shared_ptr<AudiblizerTestHarness> getptr() { return shared_from_this(); }
    
    bool Initialize();
    void PrepareForDestruction();
    
    class VideoParameters
    {
    public:
        uint32_t sampleDuration; // e.g. 1001
        uint32_t timeScale; // e.g. 30000
        uint32_t numVideoFrames; // e.g. if 10 frames, then 10 frames at 1001 / 30000 would yield a total of 0.33366667 seconds
    };
    
    typedef std::vector<VideoParameters> VideoSegments;
    bool StartTest(const VideoSegments &videoSegments, double audioPlayrateFactor = 1.0);
    bool StopTest();
    void WaitOnTestCompletion();
    
    // Audiblizer::AudioChunkCompletionListener interface
    // ------------------------------------------------------------------
    virtual void AudioChunkCompleted(const AudioChunkCompletedVector &buffersCompleted);
    
    // VideoTimerDelegate::TimerPingListener interface
    // ------------------------------------------------------------------
    virtual void VideoTimerPing();
    
    // Video FrameHead
    // ------------------------------------------------------------------
    enum PumpVideoFrameSender { PumpVideoFrameSender_VideoTimer = 0, PumpVideoFrameSender_AudioUnqueuer };
    void PumpVideoFrame(PumpVideoFrameSender sender, int32_t numPumps = 1); // stub! just outputs info
    
private:
    std::mutex mutex;
    
    std::shared_ptr<Audiblizer> audiblizer;
    std::shared_ptr<VideoTimerDelegate> videoTimerDelegate;
    std::shared_ptr<HighPrecisionTimer> highPrecisionTimer;
    
    VideoSegments videoSegments;
    uint32_t      videoSegmentsTotalNumFrames;
    uint8_t  *audioData;
    uint8_t  *audioDataPtr;
    size_t    audioDataSize;
    size_t    audioDataTotalNumDatums;
    size_t    audioDataTotalNumFrames;
    uint32_t  audioSampleRate;
    bool      audioIsStereo;
    bool      audioIsSilence;
    double    audioDurationSeconds;
    double    audioPlayrateFactor;
    const double maxQueuedAudioDurationSeconds;
    static const Audiblizer::AudioFormat audioFormat;
    
    std::mutex videoPumpMutex;
    uint64_t audioChunkIter;
    uint64_t videoFrameIter;
    uint64_t lastVideoFrameIter;
    uint64_t videoTimerIter;
    int64_t  avEqualizer; // video timer ticks ADD 1, audio buffers reclaimed SUBTRACT 1
    
    std::chrono::high_resolution_clock::time_point lastCallToPumpVideoFrame;
    std::chrono::high_resolution_clock::time_point playbackStart;
    bool firstCallToPumpVideoFrame;
    
    std::chrono::duration<float> maxDelta;
    uint64_t                     maxDeltaVideoFrameIter;
    std::chrono::duration<float> minDelta;
    uint64_t                     minDeltaVideoFrameIter;
    std::chrono::duration<float> cumulativeDelta;
    uint64_t                     numPumpsCompleted;
    bool                         videoFrameHiccup;
    uint32_t                     maxVideoFrameHiccup;
    bool                         avDrift;
    uint32_t                     avDriftNumFrames; // how many video frames reported drift from the audio
    uint32_t                     maxAVDrift;
    
    bool initialized;
    
    // --- Audio Queueing Thread ---
    std::thread *audioQueueingThread;
    bool         audioQueueingThreadRunning;
    Event        audioQueueingThreadTerminated;
    
    static void  AudioQueueingThreadProc(AudiblizerTestHarness *audiblizerTestHarness);
    
    // --- Data Output Thread ---
    class OutputData
    {
    public:
        PumpVideoFrameSender pumpVideoFrameSender;
        int64_t avEqualizer;
        int64_t audioChunkIter;
        int64_t videoFrameIter;
        std::chrono::duration<float> deltaFloatingPointSeconds;
        std::chrono::duration<float> totalFloatingPointSeconds;
    };
    
    typedef std::queue<OutputData> OutputDataQueue;
    
    OutputDataQueue outputDataQueue;
    std::mutex outputDataQueueMutex;
    
    std::thread *dataOutputThread;
    bool         dataOutputThreadRunning;
    
    static void DataOutputThreadProc(AudiblizerTestHarness *audiblizerTestHarness);
    
    // --- Static Utility Functions ---
    static void* GenerateAudioSample(uint32_t sampleRate, double durationSeconds, bool stereo, bool silence, size_t *bufferSizeOut);
    static void FreeAudioSample(void* data);
};

#endif /* AudiblizerTestHarness_h */
