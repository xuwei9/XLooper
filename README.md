# XLooper库说明

## 0.XLooper库说明

XMessage、XLooper和XHandler主要代码移植自AOSP libstagefright/foundation，支持跨平台使用。

<https://zhuanlan.zhihu.com/p/68713221>


## 1.XMessage

创建简单msg，可用于存储key-value

```javascript
shared_ptr<XMessage> msg = XMessage::obtainMsg()
```

添加key-value

```javascript
msg->setRect("myrect", 1, 2, 3, 4);
msg->setString("name", "xuwei");
```

读取key-value

```javascript
if(msg->findString("name", &name)){
    printf("name:%s\n", name.c_str());
}

int32_t w,h,l,r;
if(msg->findRect("myrect", &l, &r, &w, &h)){
    printf("w = %d,l = %d,r = %d\n",w,l,r);
}

void* value;
if(msg->findPointer("mypi", &value)){
    printf("this = %p,value = %p\n",this,value);
}
```

创建异步消息，配合looper和handler可以实现发送异步消息

```javascript
shared_ptr<XMessage> msg = XMessage::obtainMsg(tag, handler());
msg->post(delay);
```

## 2.XLooper XHandler

looper实现了一个异步线程，handler为消息处理器，子类继承XHandler，实现onMessageReceived方法。

```javascript
class testHandler:public XHandler
{
public:
    testHandler(){
        mLooper = XLooper::createLooper();
        mLooper->start();
        mLooper->registerHandler(this);
    };
    ~testHandler(){
        mLooper->stop();
        mLooper->unregisterHandler(this);
        mLooper = nullptr;
    };
    
    void fakeMsg(){
        uint32_t delay = 2000;
        uint32_t tag = 'demo';
        shared_ptr<XMessage> msg = XMessage::obtainMsg(tag, handler());
        msg->setRect("myrect", 1, 2, 3, 4);
        msg->setString("name", "xuwei");
        msg->post(delay);
    };
    
    void onMessageReceived(shared_ptr<XMessage> msg){
        printf("tag = %d\n",msg->what());
        string name;
        if(msg->findString("name", &name)){
            printf("name:%s\n", name.c_str());
        }
        int32_t w,h,l,r;
        if(msg->findRect("myrect", &l, &r, &w, &h)){
            printf("w = %d,l = %d,r = %d\n",w,l,r);
        }
    };
private:
    shared_ptr<XLooper> mLooper;
    XLooper::handler_id mLooperId;
};
```

## 3.MediaClock

基于handler和looper实现了一个媒体时钟。支持基于mediaTime的Timer事件分发功能，一般用于Video数据的同步渲染。

```javascript
    shared_ptr<testHandler> testHandler = make_shared<testHandler>();
    testHandler->init(testHandler);
    shared_ptr<MediaClock> clock = make_shared<MediaClock>();
    clock->init(clock);
    clock->updateAnchor(0, XLooper::GetNowUs());//更新clock到0秒
    clock->addTimer(XMessage::obtainMsg(33, testHandler), 10*1000*1000);//添加一个10秒钟的msg
    clock->updateAnchor(10*1000*1000, XLooper::GetNowUs());//更新clock到10秒，此时msg将弹出，
                                                           //testHandler通过onMessageReceived接收到msg
```


