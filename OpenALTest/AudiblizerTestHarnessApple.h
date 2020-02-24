//
//  AudiblizerTestHarnessApple.h
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/24/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#ifndef AudiblizerTestHarnessApple_h
#define AudiblizerTestHarnessApple_h

#include "AudiblizerTestHarness.h"

class AudiblizerTestHarnessApple : public AudiblizerTestHarness
{
public:
    AudiblizerTestHarnessApple() { }
    virtual ~AudiblizerTestHarnessApple() { }
    
    // Load sample audio from file using CoreAudio/AudioToolbox
    // ------------------------------------------------------------------
    virtual bool Load16bitStereoPCMAudioFromFile(const char *filePath, uint32_t sampleRate);
};


#endif /* AudiblizerTestHarnessApple_h */
