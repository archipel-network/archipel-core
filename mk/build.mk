include mk/toolchain.mk

# COMPONENTS

EXTERNAL_INCLUDES += -Iexternal/platform/$(PLATFORM)/include \
                     -Iexternal/tinycbor/src \
                     -Iexternal/unity/src \
                     -Iexternal/unity/extras/fixture/src \
                     -Iexternal/util/include

$(eval $(call addComponentWithRules,components/aap))
$(eval $(call addComponentWithRules,components/agents))
$(eval $(call addComponentWithRules,components/agents/$(PLATFORM)))
$(eval $(call addComponentWithRules,components/bundle6))
$(eval $(call addComponentWithRules,components/bundle7))
$(eval $(call addComponentWithRules,components/cla))
$(eval $(call addComponentWithRules,components/cla/$(PLATFORM)))
$(eval $(call addComponentWithRules,components/platform/$(PLATFORM)))
$(eval $(call addComponentWithRules,components/spp))
$(eval $(call addComponentWithRules,components/upcn))

TINYCBOR_SOURCES := \
	cborerrorstrings.c \
	cborencoder.c \
	cborencoder_close_container_checked.c \
	cborparser.c \
	cborparser_dup_string.c \
	cborpretty.c \
	cbortojson.c

$(eval $(call addComponentWithRules,external/tinycbor/src,$(TINYCBOR_SOURCES)))

$(eval $(call addComponentWithRules,external/unity/src))
$(eval $(call addComponentWithRules,external/unity/extras/fixture/src))

$(eval $(call addComponentWithRules,external/util/src))

ifeq ($(PLATFORM),stm32)

EXTERNAL_INCLUDES += -I$(FREERTOS_PREFIX)/FreeRTOS/Source/include \
                     -I$(FREERTOS_PREFIX)/FreeRTOS/Source/portable/GCC/ARM_CM4F

$(eval $(call addComponentWithRules,$(FREERTOS_PREFIX)/FreeRTOS/Source,list.c queue.c tasks.c))
$(eval $(call addComponentWithRules,$(FREERTOS_PREFIX)/FreeRTOS/Source/portable/MemMang,heap_3.c))

# Clang has to use soft floats
ifneq ($(TOOLCHAIN),clang)
  $(eval $(call addComponentWithRules,$(FREERTOS_PREFIX)/FreeRTOS/Source/portable/GCC/ARM_CM4F,port.c))
else
  $(eval $(call addComponentWithRules,$(FREERTOS_PREFIX)/FreeRTOS/Source/portable/GCC/ARM_CM3,port.c))
endif

$(eval $(call addComponentWithRules,external/platform/stm32/hwf4))
$(eval $(call addComponentWithRules,external/platform/stm32/stm32f4xx))
$(eval $(call addComponentWithRules,external/platform/stm32/usb_vcp))

endif

# LIB

$(eval $(call generateComponentRules,components/daemon))
$(eval $(call generateComponentRules,test/unit))

build/$(PLATFORM)/libupcn.so: LDFLAGS += $(LDFLAGS_LIB)
build/$(PLATFORM)/libupcn.so: LIBS = $(LIBS_libupcn.so)
build/$(PLATFORM)/libupcn.so: $(LIBS_libupcn.so) | build/$(PLATFORM)
	$(call cmd,link)

# EXECUTABLE

$(eval $(call addComponent,upcn,components/daemon))

build/$(PLATFORM)/upcn: LDFLAGS += $(LDFLAGS_EXECUTABLE)
build/$(PLATFORM)/upcn: LIBS = $(LIBS_upcn)
build/$(PLATFORM)/upcn: $(LIBS_upcn) | build/$(PLATFORM)
	$(call cmd,link)

# TEST EXECUTABLE

$(eval $(call addComponent,testupcn,test/unit))

build/$(PLATFORM)/testupcn: LDFLAGS += $(LDFLAGS_EXECUTABLE)
# 64 bit support has to be enabled first.
build/$(PLATFORM)/testupcn: CPPFLAGS += -DUNITY_SUPPORT_64
# We wrap some functions to make Unity usable without surprises on our platform.
build/$(PLATFORM)/testupcn: LDFLAGS += -Wl,-wrap,putchar \
                                       -Wl,-wrap,UNITY_OUTPUT_CHAR \
                                       -Wl,-wrap,unity_malloc \
                                       -Wl,-wrap,unity_calloc \
                                       -Wl,-wrap,unity_realloc \
                                       -Wl,-wrap,unity_free
build/$(PLATFORM)/testupcn: LIBS = $(LIBS_testupcn)
build/$(PLATFORM)/testupcn: $(LIBS_testupcn) | build/$(PLATFORM)
	$(call cmd,link)

# GENERAL RULES

build/$(PLATFORM): | build
	$(call cmd,mkdir)

build:
	$(call cmd,mkdir)
