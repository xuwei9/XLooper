//
//  XMediaClock.cpp
//  foundation
//
//  Created by xuwei on 9/23/21.
//

#include "XMediaClock.h"


/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaClock"
#include <map>
#include "XMessage.h"


#define OK (0)
// Maximum allowed time backwards from anchor change.
// If larger than this threshold, it's treated as discontinuity.
static const int64_t kAnchorFluctuationAllowedUs = 10000LL;

MediaClock::Timer::Timer(shared_ptr<XMessage> notify, int64_t mediaTimeUs, int64_t adjustRealUs)
    : mNotify(notify),
      mMediaTimeUs(mediaTimeUs),
      mAdjustRealUs(adjustRealUs) {
}

MediaClock::MediaClock()
    : mAnchorTimeMediaUs(-1),
      mAnchorTimeRealUs(-1),
      mMaxTimeMediaUs(INT64_MAX),
      mStartingTimeMediaUs(-1),
      mPlaybackRate(1.0),
      mGeneration(0),
      mNotify(NULL) {
    mLooper = XLooper::createLooper();
    mLooper->setName("MediaClock");
    mLooper->start();
}

void MediaClock::init(shared_ptr<XHandler> handler) {
    XHandler::init(handler);
    mLooper->registerHandler(this);
}

MediaClock::~MediaClock() {
    printf("~MediaClock\n");
    reset();
    if (mLooper != NULL) {
        mLooper->unregisterHandler(this);
        mLooper->stop();
    }
}

void MediaClock::reset() {
    lock_guard<mutex> autoLock(mLock);
    auto it = mTimers.begin();
    while (it != mTimers.end()) {
        it->mNotify->setInt32("reason", TIMER_REASON_RESET);
        it->mNotify->post();
        it = mTimers.erase(it);
    }
    mMaxTimeMediaUs = INT64_MAX;
    mStartingTimeMediaUs = -1;
    updateAnchorTimesAndPlaybackRate_l(-1, -1, 1.0);
    ++mGeneration;
}

void MediaClock::setStartingTimeMedia(int64_t startingTimeMediaUs) {
    lock_guard<mutex> autoLock(mLock);
    mStartingTimeMediaUs = startingTimeMediaUs;
}

void MediaClock::clearAnchor() {
    lock_guard<mutex> autoLock(mLock);
    updateAnchorTimesAndPlaybackRate_l(-1, -1, mPlaybackRate);
}

void MediaClock::updateAnchor(
        int64_t anchorTimeMediaUs,
        int64_t anchorTimeRealUs,
        int64_t maxTimeMediaUs) {
    if (anchorTimeMediaUs < 0 || anchorTimeRealUs < 0) {
        printf("reject anchor time since it is negative.");
        return;
    }

    lock_guard<mutex> autoLock(mLock);
    int64_t nowUs = XLooper::GetNowUs();
    int64_t nowMediaUs =
        anchorTimeMediaUs + (nowUs - anchorTimeRealUs) * (double)mPlaybackRate;
    if (nowMediaUs < 0) {
        printf("reject anchor time since it leads to negative media time.");
        return;
    }

    if (maxTimeMediaUs != -1) {
        mMaxTimeMediaUs = maxTimeMediaUs;
    }
    if (mAnchorTimeRealUs != -1) {
        int64_t oldNowMediaUs =
            mAnchorTimeMediaUs + (nowUs - mAnchorTimeRealUs) * (double)mPlaybackRate;
        if (nowMediaUs < oldNowMediaUs + kAnchorFluctuationAllowedUs
                && nowMediaUs > oldNowMediaUs - kAnchorFluctuationAllowedUs) {
            return;
        }
    }
    updateAnchorTimesAndPlaybackRate_l(nowMediaUs, nowUs, mPlaybackRate);

    ++mGeneration;
    processTimers_l();
}

void MediaClock::updateMaxTimeMedia(int64_t maxTimeMediaUs) {
    lock_guard<mutex> autoLock(mLock);
    mMaxTimeMediaUs = maxTimeMediaUs;
}

void MediaClock::setPlaybackRate(float rate) {
    //CHECK_GE(rate, 0.0);
    lock_guard<mutex> autoLock(mLock);
    if (mAnchorTimeRealUs == -1) {
        mPlaybackRate = rate;
        return;
    }

    int64_t nowUs = XLooper::GetNowUs();
    int64_t nowMediaUs = mAnchorTimeMediaUs + (nowUs - mAnchorTimeRealUs) * (double)mPlaybackRate;
    if (nowMediaUs < 0) {
        printf("setRate: anchor time should not be negative, set to 0.");
        nowMediaUs = 0;
    }
    updateAnchorTimesAndPlaybackRate_l(nowMediaUs, nowUs, rate);

    if (rate > 0.0) {
        ++mGeneration;
        processTimers_l();
    }
}

float MediaClock::getPlaybackRate() {
    lock_guard<mutex> autoLock(mLock);
    return mPlaybackRate;
}

int MediaClock::getMediaTime(
        int64_t realUs, int64_t *outMediaUs, bool allowPastMaxTime) {
    if (outMediaUs == NULL) {
        return -1;
    }

    lock_guard<mutex> autoLock(mLock);
    return getMediaTime_l(realUs, outMediaUs, allowPastMaxTime);
}

int MediaClock::getMediaTime_l(
        int64_t realUs, int64_t *outMediaUs, bool allowPastMaxTime) const {
    if (mAnchorTimeRealUs == -1) {
        return -2;
    }

    int64_t mediaUs = mAnchorTimeMediaUs
            + (realUs - mAnchorTimeRealUs) * (double)mPlaybackRate;
    if (mediaUs > mMaxTimeMediaUs && !allowPastMaxTime) {
        mediaUs = mMaxTimeMediaUs;
    }
    if (mediaUs < mStartingTimeMediaUs) {
        mediaUs = mStartingTimeMediaUs;
    }
    if (mediaUs < 0) {
        mediaUs = 0;
    }
    *outMediaUs = mediaUs;
    return OK;
}

int MediaClock::getRealTimeFor(
        int64_t targetMediaUs, int64_t *outRealUs) {
    if (outRealUs == NULL) {
        return -1;
    }

    lock_guard<mutex> autoLock(mLock);
    if (mPlaybackRate == 0.0) {
        return -1;
    }

    int64_t nowUs = XLooper::GetNowUs();
    int64_t nowMediaUs;
    int status =
            getMediaTime_l(nowUs, &nowMediaUs, true /* allowPastMaxTime */);
    if (status != OK) {
        return status;
    }
    *outRealUs = (targetMediaUs - nowMediaUs) / (double)mPlaybackRate + nowUs;
    return OK;
}

void MediaClock::addTimer(shared_ptr<XMessage> notify, int64_t mediaTimeUs,
                          int64_t adjustRealUs) {
    lock_guard<mutex> autoLock(mLock);

    bool updateTimer = (mPlaybackRate != 0.0);
    if (updateTimer) {
        auto it = mTimers.begin();
        while (it != mTimers.end()) {
            if (((it->mAdjustRealUs - (double)adjustRealUs) * (double)mPlaybackRate
                + (it->mMediaTimeUs - mediaTimeUs)) <= 0) {
                updateTimer = false;
                break;
            }
            ++it;
        }
    }

    mTimers.emplace_back(notify, mediaTimeUs, adjustRealUs);

    if (updateTimer) {
        ++mGeneration;
        processTimers_l();
    }
}

void MediaClock::onMessageReceived(shared_ptr<XMessage> msg) {
    switch (msg->what()) {
        case kWhatTimeIsUp:
        {
            int32_t generation;
            if(!msg->findInt32("generation", &generation))
            {
                printf("generation get failed!\n");
                break;
            }

            lock_guard<mutex> autoLock(mLock);
            if (generation != mGeneration) {
                break;
            }
            processTimers_l();
            break;
        }

        default:
            printf("should not be here!\n");
            break;
    }
}

void MediaClock::processTimers_l() {
    int64_t nowMediaTimeUs;
    int status = getMediaTime_l(
            XLooper::GetNowUs(), &nowMediaTimeUs, false /* allowPastMaxTime */);

    if (status != OK) {
        return;
    }

    int64_t nextLapseRealUs = INT64_MAX;
    std::multimap<int64_t, Timer> notifyList;
    auto it = mTimers.begin();
    while (it != mTimers.end()) {
        double diff = it->mAdjustRealUs * (double)mPlaybackRate
            + it->mMediaTimeUs - nowMediaTimeUs;
        int64_t diffMediaUs;
        if (diff > (double)INT64_MAX) {
            diffMediaUs = INT64_MAX;
        } else if (diff < (double)INT64_MIN) {
            diffMediaUs = INT64_MIN;
        } else {
            diffMediaUs = diff;
        }

        if (diffMediaUs <= 0) {
            notifyList.emplace(diffMediaUs, *it);
            it = mTimers.erase(it);
        } else {
            if (mPlaybackRate != 0.0
                && (double)diffMediaUs < (double)INT64_MAX * (double)mPlaybackRate) {
                int64_t targetRealUs = diffMediaUs / (double)mPlaybackRate;
                if (targetRealUs < nextLapseRealUs) {
                    nextLapseRealUs = targetRealUs;
                }
            }
            ++it;
        }
    }

    auto itNotify = notifyList.begin();
    while (itNotify != notifyList.end()) {
        itNotify->second.mNotify->setInt32("reason", TIMER_REASON_REACHED);
        printf("post %d\n",itNotify->second.mNotify->what());
        itNotify->second.mNotify->post();
        itNotify = notifyList.erase(itNotify);
    }

    if (mTimers.empty() || mPlaybackRate == 0.0 || mAnchorTimeMediaUs < 0
        || nextLapseRealUs == INT64_MAX) {
        return;
    }

    shared_ptr<XMessage> msg = XMessage::obtainMsg(kWhatTimeIsUp, handler());
    msg->setInt32("generation", mGeneration);
    msg->post(nextLapseRealUs);
}

void MediaClock::updateAnchorTimesAndPlaybackRate_l(int64_t anchorTimeMediaUs,
        int64_t anchorTimeRealUs, float playbackRate) {
    if (mAnchorTimeMediaUs != anchorTimeMediaUs
            || mAnchorTimeRealUs != anchorTimeRealUs
            || mPlaybackRate != playbackRate) {
        mAnchorTimeMediaUs = anchorTimeMediaUs;
        mAnchorTimeRealUs = anchorTimeRealUs;
        mPlaybackRate = playbackRate;
        notifyDiscontinuity_l();
    }
}

void MediaClock::setNotificationMessage(shared_ptr<XMessage> msg) {
    lock_guard<mutex> autoLock(mLock);
    mNotify = msg;
}

void MediaClock::notifyDiscontinuity_l() {
    if (mNotify != nullptr) {
        shared_ptr<XMessage> msg = mNotify->dup();
        msg->setInt64("anchor-media-us", mAnchorTimeMediaUs);
        msg->setInt64("anchor-real-us", mAnchorTimeRealUs);
        msg->setFloat("playback-rate", mPlaybackRate);
        msg->post();
    }
}
