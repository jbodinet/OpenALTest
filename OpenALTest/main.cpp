//
//  main.cpp
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/3/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <map>
#include <iterator>
#include "AudiblizerTestHarness.h"
#include "AudiblizerTestHarnessApple.h"

int main(int argc, const char * argv[])
{
    std::shared_ptr<AudiblizerTestHarness> audiblizerTestHarness = std::make_shared<AudiblizerTestHarnessApple>();
    AudiblizerTestHarness::VideoSegments videoSegments;
    AudiblizerTestHarness::VideoParameters videoParameters;
    double audioPlayrateFactor;
    uint32_t audioChunkCacheSize;
    uint32_t numPressureThreads;
    bool multiframerate = true;
    
    std::string sourceAudioFilePath = "/Users/josh/Desktop/04 Twisting By The Pool.m4a";
    // sourceAudioFilePath = "/Users/josh/Documents/Media/Video/Spherical/WindowsSample/SampleVideo.mp4";
    uint32_t sourceAudioSampleRate = 48000;
    
    // optionally declare and set a DataOutputter
    // ---------------------------------------
    const bool useCustomDataOutputter = false;
    if(useCustomDataOutputter)
    {
        class DataOutputter : public AudiblizerTestHarness::DataOutputter
        {
        public:
            DataOutputter() {}
            virtual ~DataOutputter() {}
            
            virtual void OutputData(const char* data)
            {
                printf("CustomDataOutputter   %s", data);
            }
        };
        
        audiblizerTestHarness->SetDataOutputter(std::make_shared<DataOutputter>());
    }
    
    // initialize test harness
    // ---------------------------------------
    if(!audiblizerTestHarness->Initialize())
    {
        printf("AudiblizerTestHarness Initialize Error!!!\n");
        goto Exit;
    }
    
    // get some type of audio into the test harness
    // ---------------------------------------
    
    // try first to load some type of test file
    if(!audiblizerTestHarness->LoadAudio(sourceAudioFilePath.c_str(), sourceAudioSampleRate))
    {
        // failing that, generate sample tone
        
        uint32_t fallbackToneSampleRate = 48000;
        bool     fallbackToneIsStereo = true;
        bool     fallbackToneIsSilence = true;
        double   fallbackTonDurationSeconds = 5.0;
        
        if(!audiblizerTestHarness->GenerateSampleAudio(fallbackToneSampleRate, fallbackToneIsStereo, fallbackToneIsSilence, fallbackTonDurationSeconds))
        {
            printf("AudiblizerTestHarness Failed to load any sample audio whatsoever!!!\n");
            goto Exit;
        }
    }
    
    // create video segment information
    // ---------------------------------------
    
    // 2 seconds of 30fps
    videoParameters.sampleDuration = 1001;
    videoParameters.timeScale = 30000;
    videoParameters.numVideoFrames = 30 * 30;
    videoSegments.push_back(videoParameters);
    
    if(multiframerate)
    {
        videoParameters.sampleDuration = 1001;
        videoParameters.timeScale = 60000;
        videoParameters.numVideoFrames = 60 * 30;
        videoSegments.push_back(videoParameters);
    }
    
    // (potentially) adversarial testing parameters
    // ---------------------------------------
    audioPlayrateFactor = 1.0;
    audioChunkCacheSize = 1;
    numPressureThreads = 0;
    
    // start test
    // ---------------------------------------
    if(!audiblizerTestHarness->StartTest(videoSegments, audioPlayrateFactor, audioChunkCacheSize, numPressureThreads))
    {
        printf("AudiblizerTestHarness StartTest Error!!!\n");
        goto Exit;
    }
    
    // wait on test completion
    audiblizerTestHarness->WaitOnTestCompletion();
    
    // stop test and report output
    // ---------------------------------------
    audiblizerTestHarness->StopTest();
    
Exit:
    return 0;
}
