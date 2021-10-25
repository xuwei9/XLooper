//
//  XMediaClock.hpp
//  foundation
//
//  Created by xuwei on 9/23/21.
//

#ifndef XMediaClock_hpp
#define XMediaClock_hpp

#include <stdio.h>
#include "XHandler.h"

class XMessage;

class MediaClock : public XHandler {
public:
    enum {
        TIMER_REASON_REACHED = 0,
        TIMER_REASON_RESET = 1,
    };

    MediaClock();
    virtual void init(shared_ptr<XHandler> handler);

    void setStartingTimeMedia(int64_t startingTimeMediaUs);

    void clearAnchor();
    // It's required to use timestamp of just rendered frame as
    // anchor time in paused state.
    void updateAnchor(
            int64_t anchorTimeMediaUs,
            int64_t anchorTimeRealUs,
            int64_t maxTimeMediaUs = INT64_MAX);

    void updateMaxTimeMedia(int64_t maxTimeMediaUs);

    void setPlaybackRate(float rate);
    float getPlaybackRate();

    // query media time corresponding to real time |realUs|, and save the
    // result in |outMediaUs|.
    int getMediaTime(
            int64_t realUs,
            int64_t *outMediaUs,
            bool allowPastMaxTime = false);
    // query real time corresponding to media time |targetMediaUs|.
    // The result is saved in |outRealUs|.
    int getRealTimeFor(int64_t targetMediaUs, int64_t *outRealUs);

    // request to set up a timer. The target time is |mediaTimeUs|, adjusted by
    // system time of |adjustRealUs|. In other words, the wake up time is
    // mediaTimeUs + (adjustRealUs / playbackRate)
    void addTimer(shared_ptr<XMessage> notify, int64_t mediaTimeUs, int64_t adjustRealUs = 0);

    void setNotificationMessage(shared_ptr<XMessage> msg);

    void reset();

    virtual ~MediaClock();
protected:


    virtual void onMessageReceived(shared_ptr<XMessage> msg);

private:
    enum {
        kWhatTimeIsUp = 'tIsU',
    };

    struct Timer {
        Timer(shared_ptr<XMessage> notify, int64_t mediaTimeUs, int64_t adjustRealUs);
        shared_ptr<XMessage> mNotify;
        int64_t mMediaTimeUs;
        int64_t mAdjustRealUs;
    };

    int getMediaTime_l(
            int64_t realUs,
            int64_t *outMediaUs,
            bool allowPastMaxTime) const;

    void processTimers_l();

    void updateAnchorTimesAndPlaybackRate_l(
            int64_t anchorTimeMediaUs, int64_t anchorTimeRealUs , float playbackRate);

    void notifyDiscontinuity_l();

    shared_ptr<XLooper> mLooper;
    mutex mLock;

    int64_t mAnchorTimeMediaUs;
    int64_t mAnchorTimeRealUs;
    int64_t mMaxTimeMediaUs;
    int64_t mStartingTimeMediaUs;

    float mPlaybackRate;

    int32_t mGeneration;
    std::list<Timer> mTimers;
    shared_ptr<XMessage> mNotify;

};

#endif /* XMediaClock_hpp */
