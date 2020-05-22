LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libfanplayer_jni

LOCAL_SRC_FILES := \
    ../../src/ffplayer.c \
    ../../src/ffrender.c \
    ../../src/pktqueue.c \
    ../../src/snapshot.c \
    ../../src/recorder.c \
    ../../src/adev-cmn.c \
    ../../src/vdev-cmn.c \
    ../../src/adev-android.cpp \
    ../../src/vdev-android.cpp \
    ../../avkcpdemuxer/ikcp.c \
    ../../avkcpdemuxer/ringbuf.c \
    ../../avkcpdemuxer/avkcpc.c \
    ../../avkcpdemuxer/avkcpd.c \
    fanplayer_jni.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/ndk-build-files/include \
    $(LOCAL_PATH)/../ffmpeg/include \
    $(LOCAL_PATH)/../soundtouch/include \
    $(LOCAL_PATH)/../../src \
    $(LOCAL_PATH)/../../avkcpdemuxer

LOCAL_CFLAGS   += -DANDROID -DNDEBUG -D__STDC_CONSTANT_MACROS -DENABLE_AVKCP_SUPPORT -Os -mfpu=neon-vfpv4 -mfloat-abi=softfp
LOCAL_CXXFLAGS += -DHAVE_PTHREADS
LOCAL_LDLIBS   += -lz -llog -landroid

LOCAL_STATIC_LIBRARIES += libavformat libavcodec libavdevice libavfilter libswresample libswscale libavutil libsoundtouch

LOCAL_MULTILIB := 32

include $(BUILD_SHARED_LIBRARY)


#++ ffmpeg prebuilt static libraries
include $(CLEAR_VARS)
LOCAL_MODULE := libavformat
LOCAL_SRC_FILES := $(LOCAL_PATH)/../ffmpeg/lib/libavformat.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavcodec
LOCAL_SRC_FILES := $(LOCAL_PATH)/../ffmpeg/lib/libavcodec.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavdevice
LOCAL_SRC_FILES := $(LOCAL_PATH)/../ffmpeg/lib/libavdevice.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavfilter
LOCAL_SRC_FILES := $(LOCAL_PATH)/../ffmpeg/lib/libavfilter.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libswresample
LOCAL_SRC_FILES := $(LOCAL_PATH)/../ffmpeg/lib/libswresample.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libswscale
LOCAL_SRC_FILES := $(LOCAL_PATH)/../ffmpeg/lib/libswscale.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavutil
LOCAL_SRC_FILES := $(LOCAL_PATH)/../ffmpeg/lib/libavutil.a
include $(PREBUILT_STATIC_LIBRARY)
#-- ffmpeg prebuilt static libraries

#++ soundtouch prebuilt static libraries
include $(CLEAR_VARS)
LOCAL_MODULE := libsoundtouch
LOCAL_SRC_FILES := $(LOCAL_PATH)/../soundtouch/lib/libsoundtouch.a
include $(PREBUILT_STATIC_LIBRARY)
#-- soundtouch prebuilt static libraries
