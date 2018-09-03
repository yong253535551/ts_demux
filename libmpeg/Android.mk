LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
		source/mpeg-crc32.c \
		source/mpeg-element-descriptor.c \
		source/mpeg-packet.c \
		source/mpeg-pack-header.c \
		source/mpeg-pat.c \
		source/mpeg-pes.c \
		source/mpeg-pmt.c \
		source/mpeg-psd.c \
		source/mpeg-ps-dec.c \
		source/mpeg-ps-enc.c \
		source/mpeg-psm.c \
		source/mpeg-system-header.c \
		source/mpeg-ts-dec.c \
		source/mpeg-ts-enc.c \
		source/mpeg-ts-h264.c \
		source/mpeg-ts-h265.c \
		source/mpeg-util.c

LOCAL_CFLAGS +=-DANDROID_LOG

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH) \
        $(LOCAL_PATH)/include 

LOCAL_SHARED_LIBRARIES += \
		libcutils \
    	libEGL \
    	libGLESv2 \
    	libGLESv1_CM \
    	libui \
    	libgui \
    	libutils \

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libmpeg

include $(BUILD_STATIC_LIBRARY)
