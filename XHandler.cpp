//
//  XHandler.cpp
//  foundation
//
//  Created by xuwei on 9/22/21.
//

#include "XHandler.h"

void XHandler::deliverMessage(shared_ptr<XMessage> msg) {
    onMessageReceived(msg);
    mMessageCounter++;
}


