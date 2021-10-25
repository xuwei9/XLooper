//
//  XLooper.hpp
//  foundation
//
//  Created by xuwei on 9/22/21.
//

#ifndef XLooper_hpp
#define XLooper_hpp

#include <stdio.h>
#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "XMessage.h"

class XHandler;
class XMessage;

using namespace std;

class XLooper
{
private:
    XLooper();
public:
    typedef int32_t event_id;
    typedef int32_t handler_id;
    static shared_ptr<XLooper> createLooper();

    virtual ~XLooper();

    static int64_t GetNowUs();
    
    struct Event {
        int64_t mWhenUs;
        shared_ptr<XMessage> mMessage;
    };

    handler_id registerHandler(XHandler *handler);
    void unregisterHandler(XHandler *handler);

    int start();
    int stop();
    
    void setName(const char *name);

private:
    friend class XMessage;
    friend class XHandler;
    void post(shared_ptr<XMessage> msg, int64_t delayUs);
    bool loop();
    void thread_func();
    
    weak_ptr<XLooper> mLooper;
    mutex mLock;
    mutex mExitLock;
    list<Event> mEventQueue;

    condition_variable mQueueChangedCondition;
    condition_variable mThreadExitedCondition;
    thread mThread;
    bool mThreadRun;
    bool mExitPending;
    string mName;
};

#endif /* XLooper_hpp */
