//
//  HighPrecisionTimer.cpp
//  HighPrecisionTimer
//
//  Created by Joshua Bodinet on 2/10/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#include "HighPrecisionTimer.h"

#include <pthread.h>
#include <sched.h>

HighPrecisionTimer::HighPrecisionTimer() :
    timerThread(nullptr),
    timerThreadRunning(false)
{
    
}

HighPrecisionTimer::~HighPrecisionTimer()
{
    Stop();
}

bool HighPrecisionTimer::Start()
{
    std::lock_guard<std::mutex> lock(timerMutex);
    
    if(timerThread != nullptr)
    {
        return false;
    }
    
    delegateSetMutex.lock();
    for(DelegateSetIterator iter = delegateSet.begin(); iter != delegateSet.end(); iter++)
    {
        iter->get()->LastPing(std::chrono::high_resolution_clock::now()); // note the time
    }
    delegateSetMutex.unlock();
    
    timerThreadRunning = true;
    timerThread = new (std::nothrow) std::thread (TimerThreadProc, this);
    if(timerThread == nullptr)
    {
        return false;
    }
    
    // set the timer thread to use a FIFO policy with top priority
    // ----------------------------------------------------------------------------------
    sched_param sch_params;
    int policy = SCHED_FIFO;
    sch_params.sched_priority = sched_get_priority_max(policy);
    int pErr = pthread_setschedparam(timerThread->native_handle(), policy, &sch_params);
    if(pErr != 0)
    {
        printf("Failed to set Thread scheduling!!! Error:%s\n", std::strerror(errno));
        
    }
    
    return true;
}

void HighPrecisionTimer::Stop()
{
    std::lock_guard<std::mutex> lock(timerMutex);
    
    if(timerThread == nullptr)
    {
        return;
    }
    
    timerThreadRunning = false;
    timerThread->join();
    delete timerThread;
    timerThread = nullptr;
    
    return;
}

bool HighPrecisionTimer::AddDelegate(std::shared_ptr<HighPrecisionTimer::Delegate> timerDelegate)
{
    std::lock_guard<std::mutex> lock(delegateSetMutex);
    
    if(timerDelegate == nullptr)
    {
        return false;
    }
    
    DelegateSetInsertionPair insertaionPair = delegateSet.insert(timerDelegate);
   
    return insertaionPair.second;
}

bool HighPrecisionTimer::RemoveDelegate(std::shared_ptr<HighPrecisionTimer::Delegate> timerDelegate)
{
    std::lock_guard<std::mutex> lock(delegateSetMutex);
    
    if(timerDelegate == nullptr)
    {
        return false;
    }
    
    DelegateSetIterator iter = delegateSet.find(timerDelegate);
    if(iter == delegateSet.end())
    {
        return false;
    }
    
    delegateSet.erase(iter);
    
    return true;
}

bool HighPrecisionTimer::RemoveAllDelegates()
{
    std::lock_guard<std::mutex> lock(delegateSetMutex);
    
    delegateSet.clear();

    return true;
}

void HighPrecisionTimer::TimerThreadProc(HighPrecisionTimer *highPrecisionTimer)
{
    if(highPrecisionTimer == nullptr)
    {
        return;
    }
    
    std::chrono::high_resolution_clock::time_point now;
    std::chrono::duration<float> deltaFloatingPointSeconds;
    
    while(highPrecisionTimer->timerThreadRunning)
    {
        highPrecisionTimer->delegateSetMutex.lock();
        for(DelegateSetIterator iter = highPrecisionTimer->delegateSet.begin(); iter != highPrecisionTimer->delegateSet.end(); iter++)
        {
            now = std::chrono::high_resolution_clock::now();
            deltaFloatingPointSeconds = now - iter->get()->LastPing();
            
            if(deltaFloatingPointSeconds.count() > iter->get()->TimerPeriod())
            {
                iter->get()->TimerPing();
                iter->get()->LastPing(now);
                
                if(iter->get()->FireOnce() || !iter->get()->Running())
                {
                    highPrecisionTimer->delegateSet.erase(iter);
                }
            }
        }
        highPrecisionTimer->delegateSetMutex.unlock();
        
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
}
                                                         
                                                         

