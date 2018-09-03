include $(all-subdir-makefiles)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
		ts_demux.cpp \
		udp_client.cpp \
		circular_buffer.cpp

LOCAL_CFLAGS +=-DANDROID_LOG

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH) \
        $(LOCAL_PATH)/../include 

LOCAL_SHARED_LIBRARIES += \
		libcutils \
    	libEGL \
    	libGLESv2 \
    	libGLESv1_CM \
    	libui \
    	libgui \
    	libutils \
		
LOCAL_STATIC_LIBRARIES += \
		libmpeg

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := ts_demux

include $(BUILD_EXECUTABLE)
