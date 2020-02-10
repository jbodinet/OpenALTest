//
//  AudiblizerTestHarness.h
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/5/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#ifndef AudiblizerTestHarness_h
#define AudiblizerTestHarness_h

#include "Audiblizer.h"
#include "Event.h"

#include <vector>
#include <queue>
#include <thread>
#include <memory>
#include <chrono>
#include <mutex>

class AudiblizerTestHarness : public Audiblizer::BufferCompletionListener, public std::enable_shared_from_this<AudiblizerTestHarness>
{
public:
    AudiblizerTestHarness();
    ~AudiblizerTestHarness();
    
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
    bool StartTest(const VideoSegments &videoSegments);
    bool StopTest();
    void WaitOnTestCompletion();
    
    virtual void BuffersCompleted(const BuffersCompletedVector &buffersCompleted);
    
    std::shared_ptr<AudiblizerTestHarness> getptr() { return shared_from_this(); }
    
private:
    std::mutex mutex;
    
    std::shared_ptr<Audiblizer> audiblizer;
    
    VideoSegments videoSegments;
    uint8_t  *audioData;
    uint8_t  *audioDataPtr;
    size_t    audioDataSize;
    size_t    audioDataTotalNumDatums;
    size_t    audioDataTotalNumFrames;
    uint32_t  audioSampleRate;
    bool      audioIsStereo;
    double    audioDurationSeconds;
    const double maxQueuedAudioDurationSeconds;
    static const Audiblizer::AudioFormat audioFormat;
    
    std::chrono::high_resolution_clock::time_point lastCallToBuffersCompleted;
    bool firstCallToBuffersCompleted;
    
    std::chrono::duration<float> maxDelta;
    std::chrono::duration<float> cumulativeDelta;
    uint64_t                     numBuffersCompleted;
    
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
        size_t numBuffers;
        std::chrono::duration<float> deltaFloatingPointSeconds;
        std::chrono::milliseconds deltaMilliseconds;
        
    };
    
    typedef std::queue<OutputData> OutputDataQueue;
    
    OutputDataQueue outputDataQueue;
    std::mutex outputDataQueueMutex;
    
    std::thread *dataOutputThread;
    bool         dataOutputThreadRunning;
    
    static void DataOutputThreadProc(AudiblizerTestHarness *audiblizerTestHarness);
    
    // --- Static Utility Functions ---
    static void* GenerateAudioSample(uint32_t sampleRate, double durationSeconds, bool stereo, size_t *bufferSizeOut);
    static void FreeAudioSample(void* data);
};

#endif /* AudiblizerTestHarness_h */
