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
$(eval $(call addComponentWithRules,components/ud3tn))

TINYCBOR_SOURCES := \
	cborerrorstrings.c \
	cborencoder.c \
	cborencoder_close_container_checked.c \
	cborparser.c \
	cborparser_dup_string.c \
	cborpretty.c \
	cborpretty_stdio.c \
	cbortojson.c

$(eval $(call addComponentWithRules,external/tinycbor/src,$(TINYCBOR_SOURCES)))

$(eval $(call addComponentWithRules,external/unity/src))
$(eval $(call addComponentWithRules,external/unity/extras/fixture/src))

$(eval $(call addComponentWithRules,external/util/src))

# LIB

$(eval $(call generateComponentRules,components/daemon))
$(eval $(call generateComponentRules,test/unit))

build/$(PLATFORM)/libud3tn.so: LDFLAGS += $(LDFLAGS_LIB)
build/$(PLATFORM)/libud3tn.so: LIBS = $(LIBS_libud3tn.so)
build/$(PLATFORM)/libud3tn.so: $(LIBS_libud3tn.so) | build/$(PLATFORM)
	$(call cmd,link)

# STATIC LIB

build/$(PLATFORM)/libud3tn.a: LIBS = $(LIBS_libud3tn.so)
build/$(PLATFORM)/libud3tn.a: $(LIBS_libud3tn.so) | build/$(PLATFORM)
	$(call cmd,arcat)

# EXECUTABLE

$(eval $(call addComponent,ud3tn,components/daemon))

build/$(PLATFORM)/ud3tn: LDFLAGS += $(LDFLAGS_EXECUTABLE)
build/$(PLATFORM)/ud3tn: LIBS = $(LIBS_ud3tn)
build/$(PLATFORM)/ud3tn: $(LIBS_ud3tn) | build/$(PLATFORM)
	$(call cmd,link)

# TEST EXECUTABLE

$(eval $(call addComponent,testud3tn,test/unit))

build/$(PLATFORM)/testud3tn: LDFLAGS += $(LDFLAGS_EXECUTABLE)
# 64 bit support has to be enabled first.
build/$(PLATFORM)/testud3tn: CPPFLAGS += -DUNITY_SUPPORT_64
# We wrap some functions to make Unity usable without surprises on our platform.
build/$(PLATFORM)/testud3tn: LDFLAGS += -Wl,-wrap,putchar \
                                       -Wl,-wrap,UNITY_OUTPUT_CHAR \
                                       -Wl,-wrap,unity_malloc \
                                       -Wl,-wrap,unity_calloc \
                                       -Wl,-wrap,unity_realloc \
                                       -Wl,-wrap,unity_free
build/$(PLATFORM)/testud3tn: LIBS = $(LIBS_testud3tn)
build/$(PLATFORM)/testud3tn: $(LIBS_testud3tn) | build/$(PLATFORM)
	$(call cmd,link)

# GENERAL RULES

build/$(PLATFORM): | build
	$(call cmd,mkdir)

build:
	$(call cmd,mkdir)
