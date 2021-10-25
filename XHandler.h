//
//  XHandler.hpp
//  foundation
//
//  Created by xuwei on 9/22/21.
//

#ifndef XHandler_hpp
#define XHandler_hpp

#include <stdio.h>
#include "XMessage.h"
#include "XLooper.h"

using namespace std;
class XMessage;

class XHandler
{
public:
    XHandler()
        :mID(0),
         mMessageCounter(0){};
    virtual ~XHandler(){};

    weak_ptr<XLooper> getLooper() {
        return mLooper;
    }
    
    XLooper::handler_id id() const {
        return mID;
    }
    
    weak_ptr<XHandler> getHandler() {
        return mHandler;
    }
    
    shared_ptr<XHandler> handler() {
        return mHandler.lock();
    }
    
    virtual void init(shared_ptr<XHandler> handler) {
        mHandler = handler;
    }

protected:
    virtual void onMessageReceived(shared_ptr<XMessage> msg) = 0;

private:
    friend class XMessage;      // deliverMessage()
    friend class XLooper;

    XLooper::handler_id mID;
    weak_ptr<XLooper> mLooper;
    weak_ptr<XHandler> mHandler;

    inline void setID(XLooper::handler_id id, weak_ptr<XLooper> looper) {
        mID = id;
        mLooper = looper;
    }

    uint32_t mMessageCounter;
    void deliverMessage(shared_ptr<XMessage> msg);
};

#endif /* XHandler_hpp */
