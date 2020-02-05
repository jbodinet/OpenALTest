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

#include <vector>
#include <thread>
#include <memory>

class AudiblizerTestHarness : public Audiblizer::BufferCompletionListener, public std::enable_shared_from_this<AudiblizerTestHarness>
{
public:
    AudiblizerTestHarness(std::shared_ptr<Audiblizer> audiblizer);
    ~AudiblizerTestHarness();
    
    void PrepareForDestruction();
    
    class VideoParameters
    {
        uint32_t sampleDuration; // e.g. 1001
        uint32_t timeScale; // e.g. 30000
        uint32_t numVideoFrames; // e.g. if 10 frames, then 10 frames at 1001 / 30000 would yield a total of 0.33366667 seconds
    };
    
    typedef std::vector<VideoParameters> VideoSegments;
    bool StartTest(const VideoSegments &videoSegments);
    bool StopTest();
    
    virtual void BuffersCompleted(const BuffersCompletedVector &buffersCompleted);
    
    std::shared_ptr<AudiblizerTestHarness> getptr() { return shared_from_this(); }
    
private:
    std::shared_ptr<Audiblizer> audiblizer;
    
    VideoSegments videoSegments;
    uint16_t *audioData;
    uint16_t *audioDataPtr;
    size_t    audioDataSize;
    
    static uint16_t* GenerateAudioSample(uint32_t sampleRate, double durationSeconds, bool stereo, size_t *bufferSizeOut);
    static void FreeAudioSample(uint16_t* data);
};

#endif /* AudiblizerTestHarness_h */
