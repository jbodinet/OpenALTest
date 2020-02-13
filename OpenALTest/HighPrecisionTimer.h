//
//  HighPrecisionTimer.h
//  HighPrecisionTimer
//
//  Created by Joshua Bodinet on 2/10/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

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
