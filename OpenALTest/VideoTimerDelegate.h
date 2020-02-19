//
//  VideoTimerDelegate.h
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/13/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#ifndef VideoTimerDelegate_h
#define VideoTimerDelegate_h

#include <iostream>

#include "HighPrecisionTimer.h"

class VideoTimerDelegate : public HighPrecisionTimer::Delegate
{
public:
    
    class TimerPingListener
    {
    public:
        TimerPingListener() { }
        virtual ~TimerPingListener() { }
        
        virtual void VideoTimerPing() = 0;
    };
    
    VideoTimerDelegate() { timerPingListener = nullptr; timerPeriod = 1001.0 / 30000.0; audioPlayrateFactor = 1.0; }
    virtual ~VideoTimerDelegate() { }
    
    virtual void SetTimerPingListener(std::shared_ptr<TimerPingListener> listener) { timerPingListener = listener; }
    virtual void PrepareForDestruction() { timerPingListener = nullptr; }
    
    virtual void SetTimerPeriod(double period) { if(period > 0) timerPeriod = period; }
    virtual void SetAudioPlayrateFactor(double factor) { if(factor > 0) audioPlayrateFactor = factor; }
    
    // HighPrecisionTimer::Delegate Interface
    // ------------------------------------------------------------------
    virtual void TimerPing() { if(timerPingListener != nullptr) timerPingListener->VideoTimerPing(); }
    virtual double TimerPeriod() { return timerPeriod * audioPlayrateFactor; }
    virtual bool FireOnce() { return false; }
    
private:
    double timerPeriod;
    double audioPlayrateFactor;
    std::shared_ptr<TimerPingListener> timerPingListener;
};

#endif /* VideoTimerDelegate_h */
