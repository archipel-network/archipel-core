LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

PLATFORM=android

LOCAL_MODULE    := archipel-core

# Components
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/components/aap/*.c)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/components/agents/*.c)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/components/bundle6/*.c)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/components/bundle7/*.c)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/components/cla/*.c)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/components/spp/*.c)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/components/ud3tn/*.c)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/components/archipel-core/*.c)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

# Tinycbor
LOCAL_SRC_FILES += $(LOCAL_PATH)/external/tinycbor/src/cborerrorstrings.c
LOCAL_SRC_FILES += $(LOCAL_PATH)/external/tinycbor/src/cborencoder.c
LOCAL_SRC_FILES += $(LOCAL_PATH)/external/tinycbor/src/cborencoder_close_container_checked.c
LOCAL_SRC_FILES += $(LOCAL_PATH)/external/tinycbor/src/cborparser.c
LOCAL_SRC_FILES += $(LOCAL_PATH)/external/tinycbor/src/cborparser_dup_string.c
LOCAL_SRC_FILES += $(LOCAL_PATH)/external/tinycbor/src/cborpretty.c
LOCAL_SRC_FILES += $(LOCAL_PATH)/external/tinycbor/src/cborpretty_stdio.c
LOCAL_SRC_FILES += $(LOCAL_PATH)/external/tinycbor/src/cbortojson.c
LOCAL_C_INCLUDES += $(LOCAL_PATH)/external/tinycbor/src

# Utils
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/external/util/src/*.c)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/external/util/include

include $(BUILD_SHARED_LIBRARY)