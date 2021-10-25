//
//  XMessage.hpp
//  foundation
//
//  Created by xuwei on 9/22/21.
//

#ifndef XMessage_hpp
#define XMessage_hpp

#include <stdio.h>
#include <stdint.h>
#include <memory>
#include "XLooper.h"

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

using namespace std;

class XHandler;
class XLooper;

class XMessage
{
private:
    XMessage();
    XMessage(uint32_t what, shared_ptr<XHandler> handler);
public:
    virtual ~XMessage();
    static shared_ptr<XMessage> obtainMsg(uint32_t what, shared_ptr<XHandler> handler);
    static shared_ptr<XMessage> obtainMsg();

    void setWhat(uint32_t what);
    uint32_t what() const;

    void setTarget(shared_ptr<XHandler> handler);

    void clear();
    int post(int64_t delayUs = 0);
    
    void setInt32(const char *name, int32_t value);
    void setInt64(const char *name, int64_t value);
    void setSize(const char *name, size_t value);
    void setFloat(const char *name, float value);
    void setDouble(const char *name, double value);
    void setPointer(const char *name, void *value);
    void setString(const char *name, const char *s, ssize_t len = -1);
    void setString(const char *name, const string &s);

    void setRect(
            const char *name,
            int32_t left, int32_t top, int32_t right, int32_t bottom);

    bool contains(const char *name) const;

    bool findInt32(const char *name, int32_t *value) const;
    bool findInt64(const char *name, int64_t *value) const;
    bool findSize(const char *name, size_t *value) const;
    bool findFloat(const char *name, float *value) const;
    bool findDouble(const char *name, double *value) const;
    bool findPointer(const char *name, void **value) const;
    bool findString(const char *name, string *value) const;
    bool findRect(const char *name,
            int32_t *left, int32_t *top, int32_t *right, int32_t *bottom) const;
    
    shared_ptr<XMessage> dup() const;


    enum Type {
        kTypeInt32,
        kTypeInt64,
        kTypeSize,
        kTypeFloat,
        kTypeDouble,
        kTypePointer,
        kTypeString,
        kTypeRect,
    };

    struct Rect {
        int32_t mLeft, mTop, mRight, mBottom;
    };
private:
    friend class XLooper;
    uint32_t mWhat;
    
    weak_ptr<XHandler> mHandler;
    weak_ptr<XLooper> mLooper;
    weak_ptr<XMessage> mMsg;
    
    struct Item {
        union {
            int32_t int32Value;
            int64_t int64Value;
            size_t sizeValue;
            float floatValue;
            double doubleValue;
            void *ptrValue;
            string *stringValue;
            Rect rectValue;
        } u;
        const char *mName;
        size_t      mNameLength;
        Type mType;
        void setName(const char *name, size_t len);
    };

    enum {
        kMaxNumItems = 64
    };
    Item mItems[kMaxNumItems];
    size_t mNumItems;

    Item *allocateItem(const char *name);
    void freeItemValue(Item *item);
    const Item *findItem(const char *name, Type type) const;
    
    size_t findItemIndex(const char *name, size_t len) const;

    void deliver();
};

#endif /* XMessage_hpp */
