cmake_minimum_required(VERSION 3.5)

cmake_policy(SET CMP0076 NEW)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

execute_process(COMMAND git describe --tags --abbrev=0 OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
project("Archipel_core" LANGUAGES C VERSION ${GIT_VERSION})

if(ESP_PLATFORM)
    include($ENV{IDF_PATH}/tools/cmake/idf.cmake)
    idf_build_process("esp32c3"
        COMPONENTS freertos log
    )
endif(ESP_PLATFORM)

# ==============
#  MARK: Archipel Core
# ==============

add_executable("archipel-core")

# Enable Archipel Core features
target_compile_definitions("archipel-core" PRIVATE "ARCHIPEL_CORE")

if(ESP_PLATFORM)
    target_compile_definitions("archipel-core" PRIVATE "ESP")
endif(ESP_PLATFORM)

# Define DEBUG in debug build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions("archipel-core" PRIVATE "DEBUG")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

# Add all includes in /include
target_include_directories("archipel-core" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# Add generated files from /generated
target_include_directories("archipel-core" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/generated")

# Add cross-platform sources files
target_sources("archipel-core" PUBLIC
    # AAP
    "components/aap/aap.c"
    "components/aap/aap_parser.c"
    "components/aap/aap_serializer.c"
    # Agents
    "components/agents/config_agent.c"
    "components/agents/config_parser.c"
    "components/agents/echo_agent.c"
    # Archipel Core
    "components/archipel-core/bundle_restore.c"
    # Bundle 6
    "components/bundle6/bundle6.c"
    "components/bundle6/create.c"
    "components/bundle6/fragment.c"
    "components/bundle6/parser.c"
    "components/bundle6/reports.c"
    "components/bundle6/sdnv.c"
    "components/bundle6/serializer.c"
    # Bundle 7
    "components/bundle7/bundle7.c"
    "components/bundle7/bundle_age.c"
    "components/bundle7/create.c"
    "components/bundle7/eid.c"
    "components/bundle7/fragment.c"
    "components/bundle7/hopcount.c"
    "components/bundle7/parser.c"
    "components/bundle7/reports.c"
    "components/bundle7/serializer.c"
    "components/bundle7/timestamp.c"
    # CLA
    "components/cla/bibe_proto.c"
    "components/cla/blackhole_parser.c"
    "components/cla/cla.c"
    "components/cla/cla_contact_rx_task.c"
    "components/cla/cla_contact_tx_task.c"
    "components/cla/mtcp_proto.c"
    # Daemon
    "components/daemon/main.c"
    # SPP
    "components/spp/spp.c"
    "components/spp/spp_internal.c"
    "components/spp/spp_parser.c"
    "components/spp/spp_timecodes.c"
    # UD3TN
    "components/ud3tn/agent_manager.c"
    "components/ud3tn/agent_util.c"
    "components/ud3tn/bundle.c"
    "components/ud3tn/bundle_fragmenter.c"
    "components/ud3tn/bundle_processor.c"
    "components/ud3tn/cmdline.c"
    "components/ud3tn/contact_manager.c"
    "components/ud3tn/crc.c"
    "components/ud3tn/eid.c"
    "components/ud3tn/init.c"
    "components/ud3tn/node.c"
    "components/ud3tn/report_manager.c"
    "components/ud3tn/router.c"
    "components/ud3tn/router_task.c"
    "components/ud3tn/routing_table.c"
    "components/ud3tn/simplehtab.c"
)

if(UNIX AND NOT ESP_PLATFORM)
    target_sources("archipel-core" PUBLIC
    # AAP2
    "components/aap2/aap2_agent.c"
    "generated/aap2/aap2.pb.c" # Generated Protobuf file
    # AAP
    "components/agents/posix/application_agent.c"
    # CLA
    "components/cla/posix/cla_bibe.c"
    "components/cla/posix/cla_file.c"
    "components/cla/posix/cla_mtcp.c"
    "components/cla/posix/cla_smtcp.c"
    "components/cla/posix/cla_tcpclv3.c"
    "components/cla/posix/cla_tcpclv3_proto.c"
    "components/cla/posix/cla_tcp_common.c"
    "components/cla/posix/cla_tcpspp.c"
    "components/cla/posix/cla_tcp_util.c"
    # Platform HAL
    "components/platform/posix/hal_io.c"
    "components/platform/posix/hal_platform.c"
    "components/platform/posix/hal_queue.c"
    "components/platform/posix/hal_semaphore.c"
    "components/platform/posix/hal_store.c"
    "components/platform/posix/hal_task.c"
    "components/platform/posix/hal_time.c"
    "components/platform/posix/pipe_queue_util.c"
    "components/platform/posix/simple_queue.c"
    "components/platform/posix/socket_util.c"
)
endif(UNIX AND NOT ESP_PLATFORM)

if(ESP_PLATFORM)
    target_sources("archipel-core" PUBLIC
    # Platform HAL
    "components/platform/esp-idf/hal_io.c"
    "components/platform/esp-idf/hal_platform.c"
    "components/platform/esp-idf/hal_queue.c"
    "components/platform/esp-idf/hal_semaphore.c"
    "components/platform/esp-idf/hal_store.c"
    "components/platform/esp-idf/hal_task.c"
    "components/platform/esp-idf/hal_time.c"
)
endif(ESP_PLATFORM)

target_link_libraries("archipel-core" "nanopb")
target_link_libraries("archipel-core" "tinycbor")
target_link_libraries("archipel-core" "util")
if(ESP_PLATFORM)
    target_link_libraries("archipel-core" "idf::freertos")
endif(ESP_PLATFORM)

# Build DEB package
if(UNIX AND NOT ESP_PLATFORM)
    install(TARGETS archipel-core RUNTIME DESTINATION "usr/bin")
    install(FILES "package/debian/archipel-core/archipel-core.service" DESTINATION "usr/lib/systemd/system" RENAME "archipel-core.service")
    install(FILES "package/debian/archipel-core/user-archipel-core.service" DESTINATION "usr/lib/systemd/user" RENAME "archipel-core.service")
    install(FILES "default-conf.env" DESTINATION "etc/archipel-core" RENAME "conf.env")
    install(FILES "LICENSE" DESTINATION "usr/share/doc/archipel-core" RENAME "copyright")

    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "archipel-core")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "EpicKiwi <me@epickiwi.fr>")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "DTN node for the Archipel Network
An experimental fork of µD3TN focused on the creation of a frugal delay tolerant network.")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/archipel-network/archipel-core")
    include(CPack)
endif(UNIX AND NOT ESP_PLATFORM)

# ==============
#  MARK: Externals
# ==============

# Externel nanopb static library (protobuf-nanopb-static)
set(nanopb_BUILD_GENERATOR OFF)
set(nanopb_MSVC_STATIC_RUNTIME OFF)
add_library("nanopb" STATIC
    "external/nanopb/pb.h"
    "external/nanopb/pb_common.h"
    "external/nanopb/pb_common.c"
    "external/nanopb/pb_encode.h"
    "external/nanopb/pb_encode.c"
    "external/nanopb/pb_decode.h"
    "external/nanopb/pb_decode.c"
)
target_include_directories("nanopb" PUBLIC "external/nanopb")

# CMake config for tinycbor
add_library("tinycbor")
target_include_directories("tinycbor" PUBLIC "external/tinycbor/src")
target_sources("tinycbor" PUBLIC 
    "external/tinycbor/src/cborencoder.c"
    "external/tinycbor/src/cborencoder_close_container_checked.c"
    "external/tinycbor/src/cborencoder_float.c"
    "external/tinycbor/src/cborerrorstrings.c"
    "external/tinycbor/src/cborparser.c"
    "external/tinycbor/src/cborparser_dup_string.c"
    "external/tinycbor/src/cborparser_float.c"
    "external/tinycbor/src/cborpretty.c"
    "external/tinycbor/src/cborpretty_stdio.c"
    "external/tinycbor/src/cbortojson.c"
    "external/tinycbor/src/cborvalidation.c"
)
if(NOT ESP_PLATFORM)
    target_sources("tinycbor" PUBLIC "external/tinycbor/src/open_memstream.c")
endif(NOT ESP_PLATFORM)

# Util library
add_library("util")
target_include_directories("util" PUBLIC "external/util/include")
target_sources("util" PUBLIC "external/util/src/htab_hash.c")

if(ESP_PLATFORM)
    idf_build_executable("archipel-core")
endif(ESP_PLATFORM)