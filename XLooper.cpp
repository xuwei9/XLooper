//
//  XLooper.cpp
//  foundation
//
//  Created by xuwei on 9/22/21.
//
#include <iostream>
#include "XLooper.h"
#include "XHandler.h"


static XLooper::handler_id mNextHandlerID = 1;

int64_t XLooper::GetNowUs() {
    auto now = chrono::steady_clock::now();
    return now.time_since_epoch().count()/1000ll;
}

shared_ptr<XLooper> XLooper::createLooper()
{

    shared_ptr<XLooper> slooper = shared_ptr<XLooper>(new XLooper());
    slooper->mLooper = slooper;
    return slooper;
}

XLooper::XLooper()
{
    mThreadRun = false;
    mExitPending = false;
}

XLooper::~XLooper()
{
    stop();
}

void XLooper::setName(const char *name) {
    mName = name;
}

XLooper::handler_id XLooper::registerHandler(XHandler *handler)
{
    lock_guard<mutex> autoLock(mLock);
    if(handler != NULL)
    {
        XLooper::handler_id handlerID = mNextHandlerID++;
        handler->setID(handlerID, mLooper.lock());
        return handlerID;
    }
    return 0;
}

void XLooper::unregisterHandler(XHandler *handler)
{
    lock_guard<mutex> autoLock(mLock);
    if(handler != NULL)
    {
        //weak_ptr<XLooper> wp;
        handler->setID(0, (shared_ptr<XLooper>)nullptr);
    }
    return;
}

void XLooper::thread_func()
{
    thread::id id = this_thread::get_id();
    std::cout <<"start thread id = " << id << endl;

    while(!mExitPending)
    {
        loop();
    }
    unique_lock<mutex> autoLock(mExitLock);
    mExitPending = true;
    mThreadRun = false;
    mThreadExitedCondition.notify_all();
    std::cout <<"exit thread id = " << id << endl;
}

int XLooper::start()
{
    lock_guard<mutex> autoLock(mLock);

    if (mThreadRun != false) {
        return -1;
    }
    mExitPending = false;
    mThreadRun = true;

    mThread = thread(&XLooper::thread_func, this);
    mThread.detach();

    return 0;
}

int XLooper::stop()
{
    if(mThreadRun == false)
    {
        printf("XLooper exit already!\n");
        return -1;
    }

    mQueueChangedCondition.notify_all();
    {
        unique_lock<mutex> autoLock(mExitLock);
        mExitPending = true;
        while(mThreadRun == true) {
            mThreadExitedCondition.wait(autoLock);
        }
        mExitPending = false;
    }

    printf("XLooper %p stoped!\n", this);
    return 0;
}

void XLooper::post(shared_ptr<XMessage> msg, int64_t delayUs) {
    lock_guard<mutex> autoLock(mLock);

    int64_t whenUs;
    if (delayUs > 0) {
        int64_t nowUs = GetNowUs();
        whenUs = (delayUs > INT64_MAX - nowUs ? INT64_MAX : nowUs + delayUs);

    } else {
        whenUs = GetNowUs();
    }

    list<Event>::iterator it = mEventQueue.begin();
    while (it != mEventQueue.end() && (*it).mWhenUs <= whenUs) {
        ++it;
    }

    Event event;
    event.mWhenUs = whenUs;
    event.mMessage = msg;

    if (it == mEventQueue.begin()) {
        mQueueChangedCondition.notify_all();
    }

    mEventQueue.insert(it, event);
}

bool XLooper::loop() {
    Event event;
    {
        unique_lock<mutex> autoLock(mLock);

        if (mEventQueue.empty()) {
            mQueueChangedCondition.wait(autoLock);
            return true;
        }
        int64_t whenUs = (*mEventQueue.begin()).mWhenUs;
        int64_t nowUs = GetNowUs();

        if (whenUs > nowUs) {
            int64_t delayUs = whenUs - nowUs;
            if (delayUs > INT64_MAX / 1000) {
                delayUs = INT64_MAX / 1000;
            }
            mQueueChangedCondition.wait_for(autoLock, chrono::milliseconds(delayUs /1000ll));

            return true;
        }

        event = *mEventQueue.begin();
        mEventQueue.erase(mEventQueue.begin());
    }

    event.mMessage->deliver();

    // NOTE: It's important to note that at this point our "ALooper" object
    // may no longer exist (its final reference may have gone away while
    // delivering the message). We have made sure, however, that loop()
    // won't be called again.

    return true;
}
