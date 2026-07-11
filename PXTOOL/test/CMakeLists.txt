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
    test_deviceagent_capability.cpp
    test_device_source.cpp
    test_diskcachesettings.cpp
    test_logsearch.cpp
    test_theme_qss.cpp
    test_upstream_demo.cpp
    test_upstream_fx2lafw.cpp
    ../pv/view/channeltint.cpp
    ../pv/dock/logsearch.cpp
    ../../libsigrok/device_source.c
    ../../libsigrok/dsdevice.c
    ../../libsigrok/log.c
    ../../libsigrok/strutil.c
    ../../common/log/xlog.c
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
)
