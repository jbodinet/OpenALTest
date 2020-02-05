//
//  Event.h
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/3/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#ifndef Event_h
#define Event_h

#include <mutex>
#include <condition_variable>
#include <chrono>

class Event
{
public:
    
    Event(bool initialState, bool manual) :
        state(initialState), manual(manual)
    {
        
    }
    
    void Signal()
    {
        std::unique_lock<std::mutex> lock(mutex);
        
        if (state)
            return;
        
        state = true;
        
        condition.notify_all();
    }
    
    void Clear()
    {
        std::unique_lock<std::mutex> lock(mutex);
        
        state = false;
    }
    
    void Wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        
        condition.wait(lock, [this] { return state; });
        
        if (!manual)
            state = false;
    }
    
    template<class Rep, class Period>
    void Wait(const std::chrono::duration<Rep, Period> &timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        
        if (!condition.wait_for(lock, timeout, [this] { return state; }))
            return;
        
        if (!manual)
            state = false;
    }
    
private:
    
    std::mutex mutex;
    std::condition_variable condition;
    bool state, manual;
};

#endif /* Event_h */
