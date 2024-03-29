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
    virtual ~AudiblizerTestHarness();
    
    std::shared_ptr<AudiblizerTestHarness> getptr() { return shared_from_this(); }
    
    virtual bool Initialize();
    virtual bool LoadAudio(const char *filePath, uint32_t sampleRate);
    virtual bool GenerateSampleAudio(uint32_t sampleRate, bool stereo, bool silence, double durationSeconds);
    virtual void PrepareForDestruction();
    
    class VideoParameters
    {
    public:
        uint32_t sampleDuration; // e.g. 1001
        uint32_t timeScale; // e.g. 30000
        uint32_t numVideoFrames; // e.g. if 10 frames, then 10 frames at 1001 / 30000 would yield a total of 0.33366667 seconds
    };
    
    typedef std::vector<VideoParameters> VideoSegments;
   
    virtual bool StartTest(const VideoSegments &videoSegments, double adversarialTestingAudioPlayrateFactor = 1.0, uint32_t adversarialTestingAudioChunkCacheSize = 1, uint32_t numAdversarialPressureTheads = 0);
    virtual bool StopTest();
    virtual void WaitOnTestCompletion();
    
    // Audiblizer::AudioChunkCompletionListener interface
    // ------------------------------------------------------------------
    virtual void AudioChunkCompleted(const AudioChunkCompletedVector &buffersCompleted);
    
    // VideoTimerDelegate::TimerPingListener interface
    // ------------------------------------------------------------------
    virtual void VideoTimerPing();
    
    // Video FrameHead
    // ------------------------------------------------------------------
    enum PumpVideoFrameSender { PumpVideoFrameSender_VideoTimer = 0, PumpVideoFrameSender_AudioUnqueuer };
    virtual void PumpVideoFrame(PumpVideoFrameSender sender, int32_t numPumps = 1); // stub! just outputs info
    
    // Load sample audio from file (must be overloaded by platform-specific / xplat-codec-specific
    // subclass in order to work)
    // ------------------------------------------------------------------
    virtual bool Load16bitStereoPCMAudioFromFile(const char *filePath, uint32_t sampleRate) { return false; }
    
    // Data Output
    // ------------------------------------------------------------------
    class DataOutputter
    {
    public:
        DataOutputter () {}
        virtual ~DataOutputter() {}
        
        virtual void OutputData(const char* data) = 0;
    };
    
    virtual void SetDataOutputter(std::shared_ptr<DataOutputter> outputter) { std::lock_guard<std::mutex> lock(dataOutputterMutex); dataOutputter = outputter; }
    
protected:
    bool initialized;
    
    std::mutex mutex;
    
    uint8_t  *audioData;
    uint8_t  *audioDataPtr;
    size_t    audioDataSize;
    size_t    audioDataTotalNumDatums;
    size_t    audioDataTotalNumFrames;
    uint32_t  audioSampleRate;
    bool      audioIsStereo;
    bool      audioIsSilence;
    double    audioDurationSeconds;
    
    // --- Static Utility Functions ---
    static void* GenerateAudioSample(uint32_t sampleRate, double durationSeconds, bool stereo, bool silence, size_t *bufferSizeOut);
    static void FreeAudioSample(void* data);
    
private:
    typedef uint64_t VideoPlaymapKey;
    typedef VideoParameters VideoPlaymapValue;
    typedef std::map<VideoPlaymapKey, VideoPlaymapValue> VideoPlaymap;
    typedef std::pair<VideoPlaymapKey, VideoPlaymapValue> VideoPlaymapPair;
    typedef VideoPlaymap::iterator VideoPlaymapIterator;
    typedef std::pair<VideoPlaymapIterator, bool> VideoPlaymapInsertionPair;
    
    std::shared_ptr<Audiblizer> audiblizer;
    std::shared_ptr<VideoTimerDelegate> videoTimerDelegate;
    std::shared_ptr<HighPrecisionTimer> highPrecisionTimer;
    
    VideoSegments videoSegments;
    uint32_t      videoSegmentsTotalNumFrames;
    VideoPlaymap  videoPlaymap;
    uint64_t      frameRateAdjustedOnFrameIndex;
    double    audioPlayrateFactor; // the actual factor of 'ideal audio playrate / actual audio playrate'
    const double maxQueuedAudioDurationSeconds;
    static const Audiblizer::AudioFormat audioFormat;
    
    std::mutex videoPumpMutex;
    uint64_t audioChunkIter;
    uint64_t videoFrameIter;
    uint64_t lastVideoFrameIter;
    uint64_t videoTimerIter;
    int64_t  avEqualizer; // video timer ticks ADD 1, audio buffers reclaimed SUBTRACT 1
    uint64_t audioRunningSlowAccum;
    const uint64_t audioRunningSlowThreshold = 3;
    
    std::chrono::high_resolution_clock::time_point lastCallToPumpVideoFrame;
    std::chrono::high_resolution_clock::time_point playbackStart;
    bool firstCallToPumpVideoFrame;
    
    class VideoSegmentOutputData
    {
    public:
        VideoSegmentOutputData()
        {
            maxDelta = std::chrono::duration<float>::zero();
            maxDeltaVideoFrameIter = 0;
            minDelta = std::chrono::duration<float>(10000.0);
            minDeltaVideoFrameIter = 0;
            cumulativeDelta = std::chrono::duration<float>::zero();
            numPumpsCompleted = 0;
            timerPeriod = 0;
        }
        
        std::chrono::duration<float> maxDelta;
        uint64_t                     maxDeltaVideoFrameIter;
        std::chrono::duration<float> minDelta;
        uint64_t                     minDeltaVideoFrameIter;
        std::chrono::duration<float> cumulativeDelta;
        uint64_t                     numPumpsCompleted;
        double                       timerPeriod;
    };
    
    std::vector<VideoSegmentOutputData> videoSegmentOutputData;
    uint32_t                            videoSegmentOutputDataIter;
    
    bool                         videoFrameHiccup;
    uint32_t                     maxVideoFrameHiccup;
    bool                         avDrift;
    uint32_t                     avDriftNumFrames; // how many video frames reported drift from the audio
    uint32_t                     maxAVDrift;
    
    std::chrono::duration<double> audioPlaybackDurationActual;
    double                        audioPlaybackDurationIdeal;
    std::chrono::high_resolution_clock::time_point lastCallToAudioChunkCompleted;
    bool                                           firstCallToAudioChunkCompleted;
    
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
        uint32_t adversarialTestingAudioChunkCacheAccum;
        int64_t videoFrameIter;
        std::chrono::duration<float> deltaFloatingPointSeconds;
        std::chrono::duration<float> totalFloatingPointSeconds;
    };
    
    std::mutex dataOutputterMutex;
    std::shared_ptr<DataOutputter> dataOutputter;
    
    // --- Pressure Threads ---
    class AdversarialPressureThread
    {
    public:
        AdversarialPressureThread();
        static std::shared_ptr<AdversarialPressureThread> CreateShared();
        
        void Kill();
        
    protected:
        bool Start();
    
    private:
        std::mutex threadMutex;
        std::thread *thread;
        bool threadRunning;
        static void AdversarialPressureThreadProc(AdversarialPressureThread *adversarialPressureThread);
    };
    
    typedef std::queue<OutputData> OutputDataQueue;
    
    OutputDataQueue outputDataQueue;
    std::mutex outputDataQueueMutex;
    
    std::thread *dataOutputThread;
    bool         dataOutputThreadRunning;
    
    static void DataOutputThreadProc(AudiblizerTestHarness *audiblizerTestHarness);
    
    // --- Adversarial Testing Components ---
    double    adversarialTestingAudioPlayrateFactor; // a way to adversarially test a/v sync by either making audio play fast or play slow
    uint32_t  adversarialTestingAudioChunkCacheSize;
    uint32_t  adversarialTestingAudioChunkCacheAccum; // we don't really need to cache the audio chunks, just collect the 'pings' and then
    std::vector<std::shared_ptr<AdversarialPressureThread>> adversarialPressureThreads;
};

#endif /* AudiblizerTestHarness_h */
