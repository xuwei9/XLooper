LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/..

LOCAL_CPPFLAGS += -O2

LOCAL_MODULE    := libxlooper
LOCAL_SRC_FILES := ../XHandler.cpp \
					../XLooper.cpp \
					../XMessage.cpp \
					../XMediaClock.cpp 					
					
 
include $(BUILD_SHARED_LIBRARY)
