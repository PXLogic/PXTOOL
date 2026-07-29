##
## This file is part of the PXTOOL project.
##
## Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

set(DSVIEW_TEST_QRC
    ../themes/breeze.qrc
)

if(Qt5Core_FOUND)
    qt5_add_resources(DSVIEW_TEST_RESOURCES ${DSVIEW_TEST_QRC})
endif()

if(Qt6Core_FOUND)
    qt6_add_resources(DSVIEW_TEST_RESOURCES ${DSVIEW_TEST_QRC})
endif()

add_executable(DSView-test
    test.cpp
    test_channeltint.cpp
    test_channelvisibility.cpp
    test_mso_mode.cpp
    test_datafeed_stub.cpp
    test_deviceagent_capability.cpp
    test_device_switch_order.cpp
    test_device_source.cpp
    test_diskcachesettings.cpp
    test_formatcapability.cpp
    test_io_options.cpp
    test_output_fixtures.cpp
    test_input_fixtures.cpp
    test_analogpacketadapter.cpp
    test_analogsnapshot.cpp
    test_analogtologic.cpp
    test_logsearch.cpp
    test_view_sampleconversion.cpp
    test_theme.cpp
    test_theme_qss.cpp
    test_upstream_demo.cpp
    test_upstream_fx2lafw.cpp
    ../pv/view/channeltint.cpp
    ../pv/dock/logsearch.cpp
    ../pv/theme/thememanager.cpp
    ${DSVIEW_TEST_RESOURCES}
    ../pv/data/formatcapability.cpp
    ../pv/data/iooptions.cpp
    ../pv/data/analogpacketadapter.cpp
    ../pv/data/snapshot.cpp
    ../pv/data/analogsnapshot.cpp
    ../pv/data/logicsnapshot.cpp
    ../pv/data/spillmanager.cpp
    ../pv/data/analogtologic.cpp
    ../pv/dialogs/inputoutputoptioneditor.cpp
    test_formatcapability_stubs.c
    test_pxlogic_stubs.c
    test_upstream_io_stubs.c
    test_deviceagent_stubs.cpp
    ../pv/deviceagent.cpp
    ../../libsigrok/analog.c
    ../../libsigrok/hardware/demo/demo.c
    ../../libsigrok/hardware/pxlogic/pxlogic.c
    ../../libsigrok/hardware/pxlogic/usb_ctrl.c
    ../../libsigrok/keyinfo.c
    ../../libsigrok/device_source.c
    ../../libsigrok/dsdevice.c
    ../../libsigrok/session.c
    ../../libsigrok/std.c
    ../../libsigrok/input/input.c
    ../../libsigrok/input/binary.c
    ../../libsigrok/input/chronovu_la8.c
    ../../libsigrok/input/csv.c
    ../../libsigrok/input/feed_queue.c
    ../../libsigrok/input/isf.c
    ../../libsigrok/input/logicport.c
    ../../libsigrok/input/null.c
    ../../libsigrok/input/protocoldata.c
    ../../libsigrok/input/raw_analog.c
    ../../libsigrok/input/saleae.c
    ../../libsigrok/input/scpi.c
    ../../libsigrok/input/stf.c
    ../../libsigrok/input/trace32_ad.c
    ../../libsigrok/input/vcd.c
    ../../libsigrok/input/wav.c
    ../../libsigrok/minilzo/minilzo.c
    ../../libsigrok/log.c
    ../../libsigrok/output/output.c
    ../../libsigrok/output/csv.c
    ../../libsigrok/output/analog.c
    ../../libsigrok/output/ascii.c
    ../../libsigrok/output/binary.c
    ../../libsigrok/output/bits.c
    ../../libsigrok/output/chronovu_la8.c
    ../../libsigrok/output/gnuplot.c
    ../../libsigrok/output/hex.c
    ../../libsigrok/output/null.c
    ../../libsigrok/output/ols.c
    ../../libsigrok/output/srzip.c
    ../../libsigrok/output/vcd.c
    ../../libsigrok/output/wav.c
    ../../libsigrok/output/wavedrom.c
    ../../libsigrok/strutil.c
    ../../common/log/xlog.c
    ../../common/minizip/unzip.c
    ../../common/minizip/ioapi.c
    ../../common/minizip/zip.c
)

if(WIN32)
    target_sources(DSView-test PRIVATE
        test_wintaskbarprogress.cpp
        ../pv/wintaskbarprogress.cpp
    )
endif()

target_compile_definitions(DSView-test PRIVATE
    DSVIEW_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# InputImporter and StoreSession operate on a real SigSession.  Their
# implementation reaches into the application runtime, so keep this coverage
# in its own headless executable instead of growing the lightweight unit test.
set(DSVIEW_FORMAT_INTEGRATION_RUNTIME_SOURCES
    ${common_SOURCES}
    ${DSView_SOURCES}
    ${DSView_HEADERS_MOC}
    ${DSView_FORMS_HEADERS}
    ${DSView_RESOURCES_RCC}
    ${libsigrok_SOURCES}
    ${libsigrokdecode_SOURCES}
)
list(REMOVE_ITEM DSVIEW_FORMAT_INTEGRATION_RUNTIME_SOURCES PXTOOL/main.cpp)

set(DSVIEW_FORMAT_INTEGRATION_RUNTIME_SOURCES_ABSOLUTE)
foreach(source_file IN LISTS DSVIEW_FORMAT_INTEGRATION_RUNTIME_SOURCES)
    if(IS_ABSOLUTE "${source_file}")
        list(APPEND DSVIEW_FORMAT_INTEGRATION_RUNTIME_SOURCES_ABSOLUTE "${source_file}")
    else()
        list(APPEND DSVIEW_FORMAT_INTEGRATION_RUNTIME_SOURCES_ABSOLUTE
            "${CMAKE_SOURCE_DIR}/${source_file}")
    endif()
endforeach()

add_executable(DSView-format-integration-test
    test_format_integration.cpp
    ${DSVIEW_FORMAT_INTEGRATION_RUNTIME_SOURCES_ABSOLUTE}
)
add_dependencies(DSView-format-integration-test ${PROJECT_NAME})

target_include_directories(DSView-format-integration-test PRIVATE
    ..
    ../..
    ../../common
    ../../libsigrok
    ${GLIB_INCLUDE_DIRS}
    ${LIBUSB_1_INCLUDE_DIRS}
    ${QT_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

target_compile_definitions(DSView-format-integration-test PRIVATE
    DSVIEW_TESTING
    DSVIEW_FORMAT_FIXTURE_PATH="${CMAKE_SOURCE_DIR}/docs/wave_data/Upstream-Compat-Demo-la-260723-120157.csv"
)

target_link_libraries(DSView-format-integration-test
    ${DSVIEW_LINK_LIBS}
    nlohmann_json::nlohmann_json
)

if(DSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO)
    target_sources(DSView-test PRIVATE
        ../../libsigrok/hardware/upstream-demo/upstream_demo.c
    )
endif()

if(DSVIEW_ENABLE_UPSTREAM_FX2LAFW)
    target_sources(DSView-test PRIVATE
        ../../libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
        ../../libsigrok/hardware/common/ezusb.c
    )
endif()

target_include_directories(DSView-test PRIVATE
    ..
    ../..
    ../../common
    ../../libsigrok
    ${GLIB_INCLUDE_DIRS}
    ${LIBUSB_1_INCLUDE_DIRS}
    ${QT_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

target_compile_definitions(DSView-test PRIVATE
    DSVIEW_TESTING
)

if(DSVIEW_ENABLE_UPSTREAM_FX2LAFW)
    target_compile_definitions(DSView-test PRIVATE
        FX2LAFW_MANIFEST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../res/fx2lafw/manifest.txt"
    )
endif()

target_link_libraries(DSView-test
    ${QT_LIBRARIES}
    -lglib-2.0
    ${LIBUSB_1_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

if(WIN32)
    target_link_libraries(DSView-test PRIVATE
        -lole32
        -luuid
    )
endif()

add_executable(srd-c-decoder-lifetime-test
    test_srd_c_decoder_lifetime.c
    ../../libsigrokdecode/type_decoder.c
    ../../libsigrokdecode/srd.c
    ../../libsigrokdecode/module_sigrokdecode.c
    ../../libsigrokdecode/decoder.c
    ../../libsigrokdecode/c_decoder_api.c
    ../../libsigrokdecode/dll_registry.c
    ../../libsigrokdecode/error.c
    ../../libsigrokdecode/exception.c
    ../../libsigrokdecode/instance.c
    ../../libsigrokdecode/log.c
    ../../libsigrokdecode/session.c
    ../../libsigrokdecode/util.c
    ../../libsigrokdecode/version.c
    ../../common/log/xlog.c
)

target_include_directories(srd-c-decoder-lifetime-test PRIVATE
    ../..
    ../../libsigrokdecode
    ${GLIB_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(srd-c-decoder-lifetime-test
    ${PY_LIB}
    -lglib-2.0
)
