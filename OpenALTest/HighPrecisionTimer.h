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

#ifndef HighPrecisionTimer_h
#define HighPrecisionTimer_h

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <mutex>
#include <set>
#include <iterator>

class HighPrecisionTimer
{
public:
    class Delegate
    {
    public:
        Delegate() : timerRunning(true) { }
        virtual ~Delegate() { }
    
        virtual void Kill() { timerRunning = false; }
        virtual bool Running() { return timerRunning; }
        virtual void LastPing(const std::chrono::high_resolution_clock::time_point &lp) { lastPing = lp; }
        virtual void RefreshLastPing() { lastPing = std::chrono::high_resolution_clock::now(); }
        virtual std::chrono::high_resolution_clock::time_point LastPing() { return lastPing; }
        
        virtual void TimerPing() = 0; // gets called when timer fires
        virtual double TimerPeriod() = 0; // in seconds
        virtual bool FireOnce() = 0; // says to fire once or multiple times
        
    private:
        bool timerRunning;
        std::chrono::high_resolution_clock::time_point lastPing;
        
        // TODO: May consider having TimerPing occur on a thread, so that a
        //       client that blocks in TimerPing does not block all other clients.
        //       HOWEVER, doing so will require even more threads, themselves which
        //       ALSO need to be near realtime threads. We should do this only if we
        //       need it, as each Delegate will require its own thread...
    };
    
    HighPrecisionTimer();
    ~HighPrecisionTimer();
    
    bool Start();
    bool AddDelegate(std::shared_ptr<HighPrecisionTimer::Delegate> timerDelegate);
    bool RemoveDelegate(std::shared_ptr<HighPrecisionTimer::Delegate> timerDelegate);
    bool RemoveAllDelegates();
    void Stop();
    
private:
    typedef std::shared_ptr<HighPrecisionTimer::Delegate> DelegateSetValue;
    typedef std::set<DelegateSetValue> DelegateSet;
    typedef DelegateSet::iterator DelegateSetIterator;
    typedef std::pair<DelegateSetIterator, bool> DelegateSetInsertionPair;
    
    DelegateSet delegateSet;
    std::mutex delegateSetMutex;
        
    std::thread *timerThread;
    bool timerThreadRunning;
    std::mutex timerMutex;
    
    static void TimerThreadProc(HighPrecisionTimer *highPrecisionTimer);
};

#endif /* HighPrecisionTimer_h */
