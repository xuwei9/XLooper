//
//  XMessage.cpp
//  foundation
//
//  Created by xuwei on 9/22/21.
//

#include "XMessage.h"
#include "XHandler.h"

shared_ptr<XMessage> XMessage::obtainMsg(uint32_t what, shared_ptr<XHandler> handler)
{
    shared_ptr<XMessage> msg = shared_ptr<XMessage>(new XMessage(what, handler));
    msg->mMsg = msg;
    return msg;
}

shared_ptr<XMessage> XMessage::obtainMsg()
{
    shared_ptr<XMessage> msg = shared_ptr<XMessage>(new XMessage());
    msg->mMsg = msg;
    return msg;
}

XMessage::XMessage(void)
    : mWhat(0){
}

XMessage::XMessage(uint32_t what, shared_ptr<XHandler> handler)
    : mWhat(what),
      mNumItems(0){
    setTarget(handler);
}

XMessage::~XMessage() {
    clear();
    printf("delete what = %d\n", mWhat);
}

void XMessage::setWhat(uint32_t what) {
    mWhat = what;
}

uint32_t XMessage::what() const {
    return mWhat;
}

void XMessage::setTarget(shared_ptr<XHandler> handler) {
    if (handler == NULL) {
        mHandler.reset();
        mLooper.reset();
    } else {
        mHandler = handler;
        mLooper = handler->getLooper();
    }
}

void XMessage::clear() {
    for (size_t i = 0; i < mNumItems; ++i) {
        Item *item = &mItems[i];
        delete[] item->mName;
        item->mName = NULL;
        freeItemValue(item);
    }
    mNumItems = 0;
}

void XMessage::deliver() {
    shared_ptr<XHandler> handler = mHandler.lock();
    if (handler == NULL) {
        printf("failed to deliver message as target handler is gone.\n");
        return;
    }

    handler->deliverMessage(mMsg.lock());
}

int XMessage::post(int64_t delayUs) {
    shared_ptr<XLooper> looper = mLooper.lock();
    if (looper == nullptr) {
        printf("failed to post message as target looper for handler is gone.\n");
        return -1;
    }

    looper->post(mMsg.lock(), delayUs);
    return 0;
}

inline size_t XMessage::findItemIndex(const char *name, size_t len) const {
    size_t i = 0;
    for (; i < mNumItems; i++) {
        if (len != mItems[i].mNameLength) {
            continue;
        }

        if (!memcmp(mItems[i].mName, name, len)) {
            break;
        }
    }
    return i;
}

// assumes item's name was uninitialized or NULL
void XMessage::Item::setName(const char *name, size_t len) {
    mNameLength = len;
    mName = new char[len + 1];
    memcpy((void*)mName, name, len + 1);
}

XMessage::Item *XMessage::allocateItem(const char *name) {
    size_t len = strlen(name);
    size_t i = findItemIndex(name, len);
    Item *item;

    if (i < mNumItems) {
        item = &mItems[i];
        freeItemValue(item);
    } else {
        if(mNumItems >= kMaxNumItems)
        {
            return NULL;
        }
        i = mNumItems++;
        item = &mItems[i];
        item->mType = kTypeInt32;
        item->setName(name, len);
    }

    return item;
}

const XMessage::Item *XMessage::findItem(
        const char *name, Type type) const {
    size_t i = findItemIndex(name, strlen(name));
    if (i < mNumItems) {
        const Item *item = &mItems[i];
        return item->mType == type ? item : NULL;

    }
    return NULL;
}

void XMessage::freeItemValue(Item *item) {
    switch (item->mType) {
        case kTypeString:
        {
            delete item->u.stringValue;
            break;
        }

        default:
            break;
    }
    item->mType = kTypeInt32; // clear type
}


#define BASIC_TYPE(NAME,FIELDNAME,TYPENAME)                             \
void XMessage::set##NAME(const char *name, TYPENAME value) {            \
    Item *item = allocateItem(name);                                    \
                                                                        \
    item->mType = kType##NAME;                                          \
    item->u.FIELDNAME = value;                                          \
}                                                                       \
                                                                        \
/* NOLINT added to avoid incorrect warning/fix from clang.tidy */       \
bool XMessage::find##NAME(const char *name, TYPENAME *value) const {  /* NOLINT */ \
    const Item *item = findItem(name, kType##NAME);                     \
    if (item) {                                                         \
        *value = item->u.FIELDNAME;                                     \
        return true;                                                    \
    }                                                                   \
    return false;                                                       \
}

BASIC_TYPE(Int32,int32Value,int32_t)
BASIC_TYPE(Int64,int64Value,int64_t)
BASIC_TYPE(Size,sizeValue,size_t)
BASIC_TYPE(Float,floatValue,float)
BASIC_TYPE(Double,doubleValue,double)
BASIC_TYPE(Pointer,ptrValue,void *)

#undef BASIC_TYPE

void XMessage::setRect(
        const char *name,
        int32_t left, int32_t top, int32_t right, int32_t bottom) {
    Item *item = allocateItem(name);
    item->mType = kTypeRect;

    item->u.rectValue.mLeft = left;
    item->u.rectValue.mTop = top;
    item->u.rectValue.mRight = right;
    item->u.rectValue.mBottom = bottom;
}

bool XMessage::findRect(
        const char *name,
        int32_t *left, int32_t *top, int32_t *right, int32_t *bottom) const {
    const Item *item = findItem(name, kTypeRect);
    if (item == NULL) {
        return false;
    }

    *left = item->u.rectValue.mLeft;
    *top = item->u.rectValue.mTop;
    *right = item->u.rectValue.mRight;
    *bottom = item->u.rectValue.mBottom;

    return true;
}

bool XMessage::findString(const char *name, string *value) const {
    const Item *item = findItem(name, kTypeString);
    if (item) {
        *value = *item->u.stringValue;
        return true;
    }
    return false;
}

void XMessage::setString(
        const char *name, const char *s, ssize_t len) {
    Item *item = allocateItem(name);
    item->mType = kTypeString;
    item->u.stringValue = new string(s, len < 0 ? strlen(s) : len);
}

void XMessage::setString(
        const char *name, const string &s) {
    setString(name, s.c_str(), s.size());
}

shared_ptr<XMessage> XMessage::dup() const {
    shared_ptr<XMessage> msg = XMessage::obtainMsg(mWhat, mHandler.lock());
    msg->mNumItems = mNumItems;


    for (size_t i = 0; i < mNumItems; ++i) {
        const Item *from = &mItems[i];
        Item *to = &msg->mItems[i];

        to->setName(from->mName, from->mNameLength);
        to->mType = from->mType;

        switch (from->mType) {
            case kTypeString:
            {
                to->u.stringValue =
                    new string(*from->u.stringValue);
                break;
            }

            default:
            {
                to->u = from->u;
                break;
            }
        }
    }

    return msg;
}
