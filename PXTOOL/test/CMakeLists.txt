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

add_executable(DSView-test
    test.cpp
    test_channeltint.cpp
    test_datafeed_stub.cpp
    test_deviceagent_capability.cpp
    test_device_source.cpp
    test_diskcachesettings.cpp
    test_formatcapability.cpp
    test_io_options.cpp
    test_output_fixtures.cpp
    test_input_fixtures.cpp
    test_analogpacketadapter.cpp
    test_logsearch.cpp
    test_theme_qss.cpp
    test_upstream_demo.cpp
    test_upstream_fx2lafw.cpp
    ../pv/view/channeltint.cpp
    ../pv/dock/logsearch.cpp
    ../pv/data/formatcapability.cpp
    test_formatcapability_stubs.c
    test_upstream_io_stubs.c
    ../../libsigrok/analog.c
    ../../libsigrok/device_source.c
    ../../libsigrok/dsdevice.c
    ../../libsigrok/session.c
    ../../libsigrok/std.c
    ../../libsigrok/input/input.c
    ../../libsigrok/input/binary.c
    ../../libsigrok/input/feed_queue.c
    ../../libsigrok/input/vcd.c
    ../../libsigrok/input/wav.c
    ../../libsigrok/log.c
    ../../libsigrok/output/output.c
    ../../libsigrok/output/csv.c
    ../../libsigrok/output/gnuplot.c
    ../../libsigrok/output/null.c
    ../../libsigrok/output/srzip.c
    ../../libsigrok/output/vcd.c
    ../../libsigrok/strutil.c
    ../../common/log/xlog.c
    ../../common/minizip/ioapi.c
    ../../common/minizip/zip.c
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
