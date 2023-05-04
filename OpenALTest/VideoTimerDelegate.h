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
