include mk/toolchain.mk

# COMPONENTS

EXTERNAL_INCLUDES += -Iexternal/nanopb \
                     -Iexternal/tinycbor/src \
                     -Iexternal/util/include \
                     -Igenerated

$(eval $(call addComponentWithRules,components/aap))
$(eval $(call addComponentWithRules,components/aap2))
$(eval $(call addComponentWithRules,components/agents))
$(eval $(call addComponentWithRules,components/agents/$(PLATFORM)))
$(eval $(call addComponentWithRules,components/bundle6))
$(eval $(call addComponentWithRules,components/bundle7))
$(eval $(call addComponentWithRules,components/cla))
$(eval $(call addComponentWithRules,components/cla/$(PLATFORM)))
$(eval $(call addComponentWithRules,components/platform/$(PLATFORM)))
$(eval $(call addComponentWithRules,components/spp))
$(eval $(call addComponentWithRules,components/ud3tn))

NANOPB_SOURCES := \
	pb_common.c \
	pb_encode.c \
	pb_decode.c

$(eval $(call addComponentWithRules,external/nanopb,$(NANOPB_SOURCES)))

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
$(eval $(call addComponentWithRules,external/util/src))

$(eval $(call addComponentWithRules,generated/aap2))

# LIB

$(eval $(call generateComponentRules,components/daemon))
$(eval $(call generateComponentRules,test/unit))
$(eval $(call generateComponentRules,test/decoder))

build/$(PLATFORM)/libud3tn.so: LIBS = $(LIBS_libud3tn.so)
build/$(PLATFORM)/libud3tn.so: $(LIBS_libud3tn.so) | build/$(PLATFORM)
	$(call cmd,linklib)

# STATIC LIB

build/$(PLATFORM)/libud3tn.a: LIBS = $(LIBS_libud3tn.so)
build/$(PLATFORM)/libud3tn.a: $(LIBS_libud3tn.so) | build/$(PLATFORM)
	$(call cmd,arcat)

# EXECUTABLE

$(eval $(call addComponent,ud3tn,components/daemon))

build/$(PLATFORM)/ud3tn: LIBS = $(LIBS_ud3tn)
build/$(PLATFORM)/ud3tn: $(LIBS_ud3tn) | build/$(PLATFORM)
	$(call cmd,link)

# TEST EXECUTABLE

$(eval $(call generateComponentRules,external/unity/src))
$(eval $(call generateComponentRules,external/unity/extras/fixture/src))

$(eval $(call addComponent,testud3tn,external/unity/src))
$(eval $(call addComponent,testud3tn,external/unity/extras/fixture/src))

$(eval $(call addComponent,testud3tn,test/unit))

build/$(PLATFORM)/testud3tn: LDFLAGS += $(LDFLAGS_EXECUTABLE)
# 64 bit support has to be enabled first.
build/$(PLATFORM)/testud3tn: CPPFLAGS += -DUNITY_SUPPORT_64
build/$(PLATFORM)/testud3tn: EXTERNAL_INCLUDES += -Itest/unit
build/$(PLATFORM)/testud3tn: EXTERNAL_INCLUDES += -Iexternal/unity/src
build/$(PLATFORM)/testud3tn: EXTERNAL_INCLUDES += -Iexternal/unity/extras/fixture/src
build/$(PLATFORM)/testud3tn: LIBS = $(LIBS_testud3tn)
build/$(PLATFORM)/testud3tn: $(LIBS_testud3tn) | build/$(PLATFORM)
	$(call cmd,link)

# DECODER EXECUTABLE

$(eval $(call addComponent,ud3tndecode,test/decoder))

build/$(PLATFORM)/ud3tndecode: build/$(PLATFORM)/libud3tn.a
build/$(PLATFORM)/ud3tndecode: LDFLAGS += $(LDFLAGS_EXECUTABLE)
build/$(PLATFORM)/ud3tndecode: LIBS = $(LIBS_ud3tndecode) build/$(PLATFORM)/libud3tn.a
build/$(PLATFORM)/ud3tndecode: $(LIBS_ud3tndecode) | build/$(PLATFORM)
	$(call cmd,link)

# GENERAL RULES

build/$(PLATFORM): | build
	$(call cmd,mkdir)

build:
	$(call cmd,mkdir)
