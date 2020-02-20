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
//#include <OpenAL/al.h>
//#include <OpenAL/alc.h>

int main(int argc, const char * argv[])
{
    std::shared_ptr<AudiblizerTestHarness> audiblizerTestHarness = std::make_shared<AudiblizerTestHarness>();
    AudiblizerTestHarness::VideoSegments videoSegments;
    AudiblizerTestHarness::VideoParameters videoParameters;
    double audioPlayrateFactor;
    uint32_t audioChunkCacheSize;
    uint32_t numPressureThreads;
    bool multiframerate = true;
    
    // initialize test harness
    // ---------------------------------------
    if(!audiblizerTestHarness->Initialize())
    {
        printf("AudiblizerTestHarness Initialize Error!!!\n");
        goto Exit;
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
