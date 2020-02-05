//
//  AudiblizerTestHarness.cpp
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/5/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#include "AudiblizerTestHarness.h"

AudiblizerTestHarness::AudiblizerTestHarness(std::shared_ptr<Audiblizer> audiblizerArg) :
    audioData(nullptr),
    audioDataPtr(nullptr),
    audioDataSize(0),
    audiblizer(audiblizerArg)
{
    audioData = GenerateAudioSample(48000, 5.0, true, &audioDataSize);
    audioDataPtr = audioData;
    
    audiblizer->SetBuffersCompletedListener(getptr());
}

AudiblizerTestHarness::~AudiblizerTestHarness()
{
    FreeAudioSample(audioData);
}

void AudiblizerTestHarness::PrepareForDestruction()
{
    StopTest();
    audiblizer = nullptr;
}

bool AudiblizerTestHarness::StartTest(const VideoSegments &videoSegments)
{
    // starts a thread. thread queues audio onto Audiblizer and records timing/feedback via AudiblizerTestHarness::BuffersCompleted
}

bool AudiblizerTestHarness::StopTest()
{
    // stops the thread
}

void AudiblizerTestHarness::BuffersCompleted(const BuffersCompletedVector &buffersCompleted)
{
    // reports time delta between calls. keeps track of rolling average delta and max and min deltas
}

uint16_t* AudiblizerTestHarness::GenerateAudioSample(uint32_t sampleRate, double durationSeconds, bool stereo, size_t *bufferSizeOut)
{
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

void AudiblizerTestHarness::FreeAudioSample(uint16_t* data)
{
    if(data != nullptr)
    {
        free(data);
        data = nullptr;
    }
}
