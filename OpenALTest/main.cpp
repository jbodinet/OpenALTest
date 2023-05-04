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
    sourceAudioFilePath = "/Users/josh/Documents/Media/Video/Spherical/WindowsSample/SampleVideo.mp4";
    //sourceAudioFilePath = "/Users/josh/Desktop/GoProHero3LaunchVideo.mp4";
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
