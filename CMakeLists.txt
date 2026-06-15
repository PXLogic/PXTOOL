##
## This file is part of the PXTOOL project.
##
## Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
## Copyright (C) 2012-2013 Alexandru Gagniuc <mr.nuke.me@gmail.com>
## Copyright (C) 2013-2022 DreamSourceLab <support@dreamsourcelab.com>
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

cmake_minimum_required(VERSION 3.5...3.27)

project(DSView)

#===============================================================================
#= Config Header
#-------------------------------------------------------------------------------

set(DS_TITLE PXTOOL)
set(DS_DESCRIPTION "A GUI for instruments of DreamSourceLab")

set(DS_VERSION_MAJOR 1)
set(DS_VERSION_MINOR 0)
set(DS_VERSION_MICRO 0)
set(DS_VERSION_STRING  ${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO})

configure_file (
	${PROJECT_SOURCE_DIR}/PXTOOL/config.h.in
	${PROJECT_BINARY_DIR}/PXTOOL/config.h
)

#===============================================================================
#=pkg config
#-------------------------------------------------------------------------------
include(FindPkgConfig)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake") 
#list(APPEND CMAKE_PREFIX_PATH "xxx.cmake find path") 

find_package(PkgConfig)

if(NOT PKG_CONFIG_FOUND)
	message(FATAL_ERROR  "Please install pkg-config!")
endif()

#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

set(DISABLE_WERROR TRUE) #Build without -Werror
set(ENABLE_SIGNALS TRUE) #Build with UNIX signals
set(ENABLE_COTIRE FALSE) #Enable cotire
option(ENABLE_TESTS "Enable unit tests" OFF)
set(STATIC_PKGDEPS_LIBS FALSE) #Statically link to (pkg-config) libraries

if(WIN32)
	# On Windows/MinGW we need to statically link to libraries.
	# This option is user configurable, but enable it by default on win32.
	set(STATIC_PKGDEPS_LIBS TRUE)

	# Windows does not support UNIX signals.
	set(ENABLE_SIGNALS FALSE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

#===============================================================================
#= first search path
#-------------------------------------------------------------------------------
include_directories(
   ./PXTOOL
   ./pv/cdecoders
   ./libsigrok
   ./libsigrokdecode
   ./common
)

#===============================================================================
#= glib-2.0
#-------------------------------------------------------------------------------
pkg_search_module(GLIB glib-2.0)

if(NOT GLIB_FOUND)
	message(FATAL_ERROR  "Please install glib!")
endif()

message("----- glib-2.0:")
message(STATUS "	 includes:" ${GLIB_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${GLIB_LIBDIR}/libglib-2.0.*)
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBDIR})

#===============================================================================
#= python3
#-------------------------------------------------------------------------------

find_package(Python3 COMPONENTS Development QUIET)

if (Python3_FOUND)
	message("----- python3:")
	message(STATUS "	 includes:" ${Python3_INCLUDE_DIRS})
	message(STATUS "	 libraries:" ${Python3_LIBRARIES})
	include_directories(${Python3_INCLUDE_DIRS})
	set(PY_LIB ${Python3_LIBRARIES})
else()
     find_package(PythonLibs 3 QUIET)

     if(PYTHONLIBS_FOUND)
     	message("----- python(${PYTHONLIBS_VERSION_STRING}):")
	message(STATUS "	 includes:" ${PYTHON_INCLUDE_DIRS})
	message(STATUS "	 libraries:" ${PYTHON_LIBRARIES})
	include_directories(${PYTHON_INCLUDE_DIRS})
	set(PY_LIB ${PYTHON_LIBRARIES})
     else()
     	message(FATAL_ERROR  "Please install lib python3!")
     endif()
endif()
  
#===============================================================================
#= FFTW
#-------------------------------------------------------------------------------
find_package(FFTW)

if(NOT FFTW_FOUND)
	message(FATAL_ERROR  "Please install lib fftw!")
endif()

message("----- FFTW:")
message(STATUS "	 includes:" ${FFTW_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${FFTW_LIBRARIES})
include_directories(${FFTW_INCLUDE_DIRS})

#===============================================================================
#= libusb-1.0
#-------------------------------------------------------------------------------
find_package(libusb-1.0)

if(NOT LIBUSB_1_FOUND)
	message(FATAL_ERROR  "Please install libusb!")
endif()

message("----- libusb-1.0:")
message(STATUS "	 includes:" ${LIBUSB_1_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${LIBUSB_1_LIBRARIES})
include_directories(${LIBUSB_1_INCLUDE_DIRS})

#===============================================================================
#= zlib
#-------------------------------------------------------------------------------
find_package(ZLIB QUIET)

if(NOT ZLIB_FOUND)
	message(FATAL_ERROR  "Please install zlib!")
endif()

message("----- zlib:")
message(STATUS "	 includes:" ${ZLIB_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${ZLIB_LIBRARIES})
include_directories(${ZLIB_INCLUDE_DIRS})

#===============================================================================
#= Qt5 or Qt6
#-------------------------------------------------------------------------------
if(APPLE)
	set(QT_VERSION_FORCE Qt6)
endif()


set(QT_VERSION_FORCE "" CACHE STRING "Force the use of Qt6 by setting this option to 6")

if(NOT QT_VERSION_FORCE OR "${QT_VERSION_FORCE}" STREQUAL "5")
	find_package(Qt5Core QUIET)
	if(Qt5Core_FOUND)
		message("----- Qt5:")
		message(STATUS "	 includes:" ${Qt5Core_INCLUDE_DIRS})
		#find_package(Qt5WinExtras REQUIRED)
		find_package(Qt5Widgets REQUIRED)
		find_package(Qt5Gui REQUIRED)
		find_package(Qt5Svg REQUIRED)
		find_package(Qt5Network REQUIRED)
		# win32
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
		if (WIN32)
			find_package(Qt5WinExtras REQUIRED)
			message(STATUS "	 libraries:" ${Qt5WinExtras_LIBRARIES})
			set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5WinExtras_INCLUDE_DIRS} ${Qt5Svg_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS})
			set(QT_LIBRARIES Qt5::Gui Qt5::Widgets Qt5::WinExtras Qt5::Svg Qt5::Network)
			add_definitions(${Qt5Gui_DEFINITIONS} ${Qt5Widgets_DEFINITIONS} ${Qt5WinExtras_DEFINITIONS} ${Qt5Svg_DEFINITIONS} ${Qt5Network_DEFINITIONS})
		else()
			set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS})
			set(QT_LIBRARIES Qt5::Gui Qt5::Widgets Qt5::Network)
			add_definitions(${Qt5Gui_DEFINITIONS} ${Qt5Widgets_DEFINITIONS} ${Qt5Network_DEFINITIONS})
		endif()
	endif()
endif()

if(NOT Qt5Core_FOUND OR "${QT_VERSION_FORCE}" STREQUAL "6")
	find_package(Qt6 REQUIRED COMPONENTS Core)
    message("----- Qt6:")
	message(STATUS "	 includes:" ${Qt6Core_INCLUDE_DIRS})
        find_package(Qt6Widgets REQUIRED)
        find_package(Qt6Gui REQUIRED)
        find_package(Qt6Svg REQUIRED)
        find_package(Qt6Network REQUIRED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt6Widgets_EXECUTABLE_COMPILE_FLAGS}")
	set(QT_INCLUDE_DIRS ${Qt6Gui_INCLUDE_DIRS} ${Qt6Widgets_INCLUDE_DIRS} ${Qt6Network_INCLUDE_DIRS})
        set(QT_LIBRARIES Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Network)
	add_definitions(${Qt6Gui_DEFINITIONS} ${Qt6Widgets_DEFINITIONS} ${Qt6Network_DEFINITIONS})

	if(APPLE)
		find_package(Qt6DBus REQUIRED)
		set(QT_LIBRARIES ${QT_LIBRARIES} Qt6::DBus)
	endif()
endif()

if(NOT Qt5Core_FOUND AND NOT Qt6Core_FOUND)
	message("Error!The qt5 or qt6 can't find, if them has been installed, please append the install path to CMAKE_PREFIX_PATH, the command as:")
	message("list(APPEND CMAKE_PREFIX_PATH \"xxx install path\")" )
	message(FATAL_ERROR  "Can't find Qt5 or Qt6!")
endif()

find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

#===============================================================================
#= boost
#-------------------------------------------------------------------------------
find_package(Boost 1.42 QUIET)

if(NOT Boost_FOUND)
	message(FATAL_ERROR  "Please install boost!")
endif()

message("----- boost:")
message(STATUS "	 includes:" ${Boost_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

#===============================================================================
#= Dependencies
#-------------------------------------------------------------------------------

find_package(Threads)

#===============================================================================
#= DSView sources
#-------------------------------------------------------------------------------

set(DSView_SOURCES
    PXTOOL/main.cpp
    PXTOOL/application.cpp
    PXTOOL/pv/log.cpp
    PXTOOL/pv/api/mcp_transport.cpp
    PXTOOL/pv/sigsession.cpp
    PXTOOL/pv/mainwindow.cpp
    PXTOOL/pv/data/snapshot.cpp
    PXTOOL/pv/data/signaldata.cpp
    PXTOOL/pv/data/logicsnapshot.cpp
    PXTOOL/pv/data/spillmanager.cpp
    PXTOOL/pv/data/analogsnapshot.cpp
    PXTOOL/pv/dialogs/deviceoptions.cpp
    PXTOOL/pv/prop/property.cpp
    PXTOOL/pv/prop/int.cpp
    PXTOOL/pv/prop/enum.cpp
    PXTOOL/pv/prop/double.cpp
    PXTOOL/pv/prop/bool.cpp
    PXTOOL/pv/prop/binding/binding.cpp
    PXTOOL/pv/toolbars/samplingbar.cpp
    PXTOOL/pv/view/viewport.cpp
    PXTOOL/pv/view/view.cpp
    PXTOOL/pv/view/timemarker.cpp
    PXTOOL/pv/view/signal.cpp
    PXTOOL/pv/view/ruler.cpp
    PXTOOL/pv/view/waveformscrollbar.cpp
    PXTOOL/pv/view/verticalscrollbar.cpp
    PXTOOL/pv/view/logicsignal.cpp
    PXTOOL/pv/view/header.cpp
    PXTOOL/pv/view/cursor.cpp
    PXTOOL/pv/view/analogsignal.cpp
    PXTOOL/pv/prop/binding/deviceoptions.cpp
    PXTOOL/pv/toolbars/trigbar.cpp
    PXTOOL/pv/toolbars/filebar.cpp
    PXTOOL/pv/dock/protocoldock.cpp
    PXTOOL/pv/dock/triggerdock.cpp
    PXTOOL/pv/dock/measuredock.cpp
    PXTOOL/pv/dock/searchdock.cpp
    PXTOOL/pv/dock/sidebar.cpp
    PXTOOL/pv/dock/glitchfilterdock.cpp
    PXTOOL/pv/dock/deviceoptionsdock.cpp
    PXTOOL/pv/toolbars/logobar.cpp
    PXTOOL/pv/dialogs/about.cpp
    PXTOOL/pv/dialogs/diskcachedialog.cpp
    PXTOOL/pv/dialogs/search.cpp
    PXTOOL/pv/data/dsosnapshot.cpp
    PXTOOL/pv/data/glitchfilter.cpp
    PXTOOL/pv/view/dsosignal.cpp
    PXTOOL/pv/view/dsldial.cpp
    PXTOOL/pv/dock/dsotriggerdock.cpp
    PXTOOL/pv/view/trace.cpp
    PXTOOL/pv/view/selectableitem.cpp
    PXTOOL/pv/data/decoderstack.cpp
    PXTOOL/pv/data/decode/rowdata.cpp
    PXTOOL/pv/data/decode/row.cpp
    PXTOOL/pv/data/decode/decoder.cpp
    PXTOOL/pv/data/decode/annotation.cpp
    PXTOOL/pv/view/decodetrace.cpp
    pv/cdecoders/c_decoder_registry.cpp
    PXTOOL/pv/prop/binding/decoderoptions.cpp
    PXTOOL/pv/widgets/fakelineedit.cpp
    PXTOOL/pv/widgets/searchedgeflagedit.cpp
    PXTOOL/pv/widgets/decodermenu.cpp
    PXTOOL/pv/widgets/decodergroupbox.cpp
    PXTOOL/pv/prop/string.cpp
    PXTOOL/pv/dialogs/storeprogress.cpp
    PXTOOL/pv/storesession.cpp
    PXTOOL/pv/view/devmode.cpp
    PXTOOL/pv/dialogs/waitingdialog.cpp
    PXTOOL/pv/dialogs/dsomeasure.cpp
    PXTOOL/pv/dialogs/calibration.cpp
    PXTOOL/pv/data/decodermodel.cpp
    PXTOOL/pv/dialogs/protocollist.cpp
    PXTOOL/pv/dialogs/protocolexp.cpp
    PXTOOL/pv/dialogs/fftoptions.cpp
    PXTOOL/pv/data/mathstack.cpp
    PXTOOL/pv/view/mathtrace.cpp
    PXTOOL/pv/toolbars/titlebar.cpp
    PXTOOL/pv/mainframe.cpp
    PXTOOL/pv/widgets/border.cpp
    PXTOOL/pv/dialogs/dsmessagebox.cpp
    PXTOOL/pv/dialogs/shadow.cpp
    PXTOOL/pv/dialogs/dsdialog.cpp
    PXTOOL/pv/dialogs/interval.cpp
    PXTOOL/pv/prop/binding/probeoptions.cpp
    PXTOOL/pv/view/viewstatus.cpp
    PXTOOL/pv/dialogs/lissajousoptions.cpp
    PXTOOL/pv/view/lissajoustrace.cpp
    PXTOOL/pv/view/spectrumtrace.cpp
    PXTOOL/pv/data/spectrumstack.cpp
    PXTOOL/pv/dialogs/mathoptions.cpp
    PXTOOL/pv/dialogs/regionoptions.cpp
    PXTOOL/pv/view/xcursor.cpp
    PXTOOL/pv/dock/protocoldock.cpp
    PXTOOL/pv/data/decoderstack.cpp
    PXTOOL/pv/data/decode/annotation.cpp
    PXTOOL/pv/data/decode/decoder.cpp
    PXTOOL/pv/data/decode/row.cpp
    PXTOOL/pv/data/decode/rowdata.cpp
    PXTOOL/pv/prop/binding/decoderoptions.cpp
    PXTOOL/pv/view/decodetrace.cpp
    PXTOOL/pv/widgets/decodergroupbox.cpp
    PXTOOL/pv/widgets/decodermenu.cpp
    PXTOOL/pv/config/appconfig.cpp
    PXTOOL/pv/appcontrol.cpp
    PXTOOL/pv/dstimer.cpp
    PXTOOL/pv/eventobject.cpp
    PXTOOL/pv/ZipMaker.cpp
    PXTOOL/pv/data/decode/annotationrestable.cpp
    PXTOOL/pv/data/decode/decoderstatus.cpp
    PXTOOL/pv/dock/protocolitemlayer.cpp
    PXTOOL/pv/ui/msgbox.cpp
    PXTOOL/pv/ui/dscombobox.cpp
    PXTOOL/pv/dsvdef.cpp
    PXTOOL/pv/dialogs/applicationpardlg.cpp
    PXTOOL/pv/dock/keywordlineedit.cpp
    PXTOOL/pv/dock/searchcombobox.cpp
    PXTOOL/pv/dialogs/decoderoptionsdlg.cpp
    PXTOOL/pv/utility/encoding.cpp
    PXTOOL/pv/utility/path.cpp
    PXTOOL/pv/utility/array.cpp
    PXTOOL/pv/deviceagent.cpp
    PXTOOL/pv/ui/langresource.cpp
    PXTOOL/pv/ui/fn.cpp
    PXTOOL/pv/ui/xtoolbutton.cpp
    PXTOOL/pv/ui/uimanager.cpp
    PXTOOL/pv/ui/popupdlglist.cpp
    PXTOOL/pv/dock/logdock.cpp
    PXTOOL/pv/dock/logsearch.cpp
    PXTOOL/pv/sessioncallback.cpp
    PXTOOL/pv/dialogs/shortcutdlg.cpp
)

# Windows-specific QT source files
if(WIN32)
	list(APPEND DSView_SOURCES
		PXTOOL/pv/winnativewidget.cpp
		PXTOOL/pv/winshadow.cpp
	)
endif ()

set(DSView_HEADERS
    PXTOOL/mystyle.h
    PXTOOL/pv/log.h
    PXTOOL/pv/api/types.h
    PXTOOL/pv/api/transport.h
    PXTOOL/pv/api/mcp_transport.h
    PXTOOL/pv/sigsession.h
    PXTOOL/pv/mainwindow.h
    PXTOOL/pv/dialogs/deviceoptions.h
    PXTOOL/pv/data/spillmanager.h
    PXTOOL/pv/utility/diskcachesettings.h
    PXTOOL/pv/prop/property.h
    PXTOOL/pv/prop/int.h
    PXTOOL/pv/prop/enum.h
    PXTOOL/pv/prop/double.h
    PXTOOL/pv/prop/bool.h
    PXTOOL/pv/toolbars/samplingbar.h
    PXTOOL/pv/view/viewport.h
    PXTOOL/pv/view/view.h
    PXTOOL/pv/view/timemarker.h
    PXTOOL/pv/view/ruler.h
    PXTOOL/pv/view/waveformscrollbar.h
    PXTOOL/pv/view/verticalscrollbar.h
    PXTOOL/pv/view/header.h
    PXTOOL/pv/view/cursor.h
    PXTOOL/pv/toolbars/trigbar.h
    PXTOOL/pv/toolbars/filebar.h
    PXTOOL/pv/dock/protocoldock.h
    PXTOOL/pv/dock/triggerdock.h
    PXTOOL/pv/dock/measuredock.h
    PXTOOL/pv/dock/searchdock.h
    PXTOOL/pv/dock/sidebar.h
    PXTOOL/pv/dock/glitchfilterdock.h
    PXTOOL/pv/data/glitchfilter.h
    PXTOOL/pv/dock/deviceoptionsdock.h
    PXTOOL/pv/toolbars/logobar.h
    PXTOOL/pv/dialogs/about.h
    PXTOOL/pv/dialogs/diskcachedialog.h
    PXTOOL/pv/dialogs/search.h
    PXTOOL/pv/dock/dsotriggerdock.h
    PXTOOL/pv/view/trace.h
    PXTOOL/pv/view/selectableitem.h
    PXTOOL/pv/data/decoderstack.h
    PXTOOL/pv/view/decodetrace.h
    PXTOOL/pv/widgets/fakelineedit.h
    PXTOOL/pv/widgets/searchedgeflagedit.h
    PXTOOL/pv/widgets/decodermenu.h
    PXTOOL/pv/widgets/decodergroupbox.h
    PXTOOL/pv/prop/string.h
    PXTOOL/pv/dialogs/storeprogress.h
    PXTOOL/pv/storesession.h
    PXTOOL/pv/view/devmode.h
    PXTOOL/pv/dialogs/waitingdialog.h
    PXTOOL/pv/dialogs/dsomeasure.h
    PXTOOL/pv/dialogs/calibration.h
    PXTOOL/pv/dialogs/protocollist.h
    PXTOOL/pv/dialogs/protocolexp.h
    PXTOOL/pv/dialogs/fftoptions.h
    PXTOOL/pv/data/mathstack.h
    PXTOOL/pv/view/mathtrace.h
    PXTOOL/pv/view/viewstatus.h
    PXTOOL/pv/toolbars/titlebar.h
    PXTOOL/pv/mainframe.h
    PXTOOL/pv/widgets/border.h
    PXTOOL/pv/dialogs/dsmessagebox.h
    PXTOOL/pv/dialogs/shadow.h
    PXTOOL/pv/dialogs/dsdialog.h
    PXTOOL/pv/dialogs/interval.h
    PXTOOL/pv/dialogs/lissajousoptions.h
    PXTOOL/pv/view/lissajoustrace.h
    PXTOOL/pv/view/spectrumtrace.h
    PXTOOL/pv/data/spectrumstack.h
    PXTOOL/pv/dialogs/mathoptions.h
    PXTOOL/pv/dialogs/regionoptions.h
    PXTOOL/pv/view/xcursor.h
    PXTOOL/pv/view/signal.h
    PXTOOL/pv/view/logicsignal.h
    PXTOOL/pv/view/analogsignal.h
    PXTOOL/pv/view/dsosignal.h
    PXTOOL/pv/dock/protocoldock.h
    PXTOOL/pv/data/decoderstack.h
    PXTOOL/pv/view/decodetrace.h
    PXTOOL/pv/widgets/decodergroupbox.h
    PXTOOL/pv/widgets/decodermenu.h
    PXTOOL/pv/config/appconfig.h
    PXTOOL/pv/appcontrol.h
    PXTOOL/pv/dstimer.h
    PXTOOL/pv/eventobject.h
    PXTOOL/pv/ZipMaker.h
    PXTOOL/pv/data/decode/annotationrestable.h
    PXTOOL/pv/data/decode/decoderstatus.h
    PXTOOL/pv/dock/protocolitemlayer.h
    PXTOOL/pv/ui/msgbox.h
    PXTOOL/pv/ui/dscombobox.h
    PXTOOL/pv/dsvdef.h
    PXTOOL/pv/dialogs/applicationpardlg.h
    PXTOOL/pv/dock/keywordlineedit.h
    PXTOOL/pv/dock/searchcombobox.h
    PXTOOL/pv/dialogs/decoderoptionsdlg.h
    PXTOOL/pv/utility/encoding.h
    PXTOOL/pv/utility/path.h
    PXTOOL/pv/utility/array.h
    PXTOOL/pv/deviceagent.h
    PXTOOL/pv/ui/fn.h
    PXTOOL/pv/ui/xtoolbutton.h
    PXTOOL/pv/dock/logdock.h
    PXTOOL/pv/dock/logsearch.h
    PXTOOL/pv/dialogs/shortcutdlg.h
)
 
# Windows-specific QT headers
if(WIN32)
	list(APPEND DSView_HEADERS
		PXTOOL/pv/winnativewidget.h
		PXTOOL/pv/winshadow.h
	)
endif ()

#===============================================================================
#= libsigrok source
#-------------------------------------------------------------------------------
set(libsigrok_SOURCES
    libsigrok/version.c
    libsigrok/strutil.c
    libsigrok/std.c
    libsigrok/session_driver.c
    libsigrok/session.c
    libsigrok/log.c
    libsigrok/hwdriver.c
    libsigrok/error.c
    libsigrok/backend.c
    libsigrok/output/output.c
    libsigrok/input/input.c
    libsigrok/hardware/demo/demo.c
    libsigrok/input/in_binary.c
    libsigrok/input/in_vcd.c
    libsigrok/input/in_wav.c
    libsigrok/output/csv.c
    libsigrok/output/gnuplot.c
    libsigrok/output/srzip.c
    libsigrok/output/vcd.c
    libsigrok/hardware/DSL/dslogic.c
    libsigrok/hardware/common/usb.c
    libsigrok/hardware/common/ezusb.c
    libsigrok/trigger.c
    libsigrok/dsdevice.c
    libsigrok/hardware/DSL/dscope.c
    libsigrok/hardware/DSL/command.c
    libsigrok/hardware/DSL/dsl.c
    libsigrok/hardware/pxlogic/pxlogic.c
    libsigrok/hardware/pxlogic/usb_ctrl.c
    libsigrok/lib_main.c
)

set(libsigrok_HEADERS
    libsigrok/version.h
    libsigrok/libsigrok-internal.h
    libsigrok/libsigrok.h
    libsigrok/config.h
    libsigrok/hardware/DSL/command.h
    libsigrok/hardware/DSL/dsl.h
)
 
#===============================================================================
#= libsigrokdecode source
#-------------------------------------------------------------------------------
set(libsigrokdecode_SOURCES
    libsigrokdecode/type_decoder.c
    libsigrokdecode/srd.c
    libsigrokdecode/module_sigrokdecode.c
    libsigrokdecode/decoder.c
    libsigrokdecode/c_decoder_api.c
    libsigrokdecode/dll_registry.c
    libsigrokdecode/error.c
    libsigrokdecode/exception.c
    libsigrokdecode/instance.c
    libsigrokdecode/log.c
    libsigrokdecode/session.c
    libsigrokdecode/util.c
    libsigrokdecode/version.c
)

set(libsigrokdecode_HEADERS
    libsigrokdecode/libsigrokdecode-internal.h
    libsigrokdecode/libsigrokdecode.h
    libsigrokdecode/dll_registry.h
    libsigrokdecode/config.h
    libsigrokdecode/version.h
)

#===============================================================================
#= common source
#-------------------------------------------------------------------------------

set(common_SOURCES
    common/minizip/zip.c
    common/minizip/unzip.c
    common/minizip/ioapi.c
    common/log/xlog.c
)

set(common_HEADERS
    common/minizip/zip.h
    common/minizip/unzip.h
    common/minizip/ioapi.h
    common/log/xlog.h
)

#===============================================================================
#= compile config
#-------------------------------------------------------------------------------

set(DSView_FORMS
)

set(DSView_RESOURCES
    PXTOOL/PXTOOL.qrc
    PXTOOL/themes/breeze.qrc
    PXTOOL/languages/language.qrc
)


if(WIN32)
	# Use the app icon for the PXTOOL.exe executable.
	set(CMAKE_RC_COMPILE_OBJECT "${CMAKE_RC_COMPILER} -O coff -I${CMAKE_CURRENT_SOURCE_DIR} <SOURCE> <OBJECT>")
	enable_language(RC)
	# app icon
        list(APPEND DSView_SOURCES applogo.rc)
	# Rebuild the .rc object when the .ico changes (Make does not track this by default).
	set_source_files_properties(applogo.rc PROPERTIES
		OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/win-app-logo.ico")
	# MinGW Unicode support
	add_definitions(-municode)
endif()

# Qt Linguist: compile zh_CN.ts → zh_CN.qm during build
if(Qt5Core_FOUND)
	find_package(Qt5 COMPONENTS LinguistTools QUIET)
elseif(Qt6Core_FOUND)
	find_package(Qt6 COMPONENTS LinguistTools QUIET)
endif()

set(DSVIEW_TS_FILES
    PXTOOL/languages/zh_CN.ts
    PXTOOL/languages/zh_TW.ts
)
if(Qt5LinguistTools_FOUND)
	qt5_add_translation(DSVIEW_QM_FILES ${DSVIEW_TS_FILES})
elseif(Qt6LinguistTools_FOUND)
	qt6_add_translation(DSVIEW_QM_FILES ${DSVIEW_TS_FILES})
endif()

if(Qt5Core_FOUND)
	qt5_wrap_cpp(DSView_HEADERS_MOC ${DSView_HEADERS})
	qt5_wrap_ui(DSView_FORMS_HEADERS ${DSView_FORMS})
	qt5_add_resources(DSView_RESOURCES_RCC ${DSView_RESOURCES})
endif()

if(Qt6Core_FOUND)
	qt6_wrap_cpp(DSView_HEADERS_MOC ${DSView_HEADERS})
	qt6_wrap_ui(DSView_FORMS_HEADERS ${DSView_FORMS})
	qt6_add_resources(DSView_RESOURCES_RCC ${DSView_RESOURCES})
endif()


add_definitions(${QT_DEFINITIONS})
add_definitions(-Wall -Wextra -Wno-return-type -Wno-ignored-qualifiers)

if(NOT DISABLE_WERROR)
        add_definitions(-Werror)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")


include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${Boost_INCLUDE_DIRS}
	${QT_INCLUDE_DIRS}
)


if(STATIC_PKGDEPS_LIBS)
	include_directories(${PKGDEPS_STATIC_INCLUDE_DIRS})
else()
	include_directories(${PKGDEPS_INCLUDE_DIRS})
endif()

#===============================================================================
#= Release flags
#-------------------------------------------------------------------------------
if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
    add_compile_options(-O3)
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-g")

#===============================================================================
#= Linker Configuration
#-------------------------------------------------------------------------------


set(DSVIEW_LINK_LIBS
	-lz
	-lglib-2.0
	${CMAKE_THREAD_LIBS_INIT}
	${QT_LIBRARIES}
	${LIBUSB_1_LIBRARIES}
	${FFTW_LIBRARIES}
	${PY_LIB}
)

if(NOT APPLE)
    list(APPEND DSVIEW_LINK_LIBS -ldl)
endif()

if(STATIC_PKGDEPS_LIBS)
	link_directories(${PKGDEPS_STATIC_LIBRARY_DIRS})
	list(APPEND DSVIEW_LINK_LIBS ${PKGDEPS_STATIC_LIBRARIES})
if(WIN32)
	# Workaround for a MinGW linking issue.
	list(APPEND PULSEVIEW_LINK_LIBS "-llzma -llcms2")
endif()
else()
	link_directories(${PKGDEPS_LIBRARY_DIRS})
	list(APPEND DSVIEW_LINK_LIBS ${PKGDEPS_LIBRARIES})
endif()

add_executable(${PROJECT_NAME}
	${common_SOURCES}
	${DSView_SOURCES}
	${DSView_HEADERS_MOC}
	${DSView_FORMS_HEADERS}
	${DSView_RESOURCES_RCC}
	${libsigrok_SOURCES}
	${libsigrokdecode_SOURCES}
)

target_link_libraries(${PROJECT_NAME} ${DSVIEW_LINK_LIBS} nlohmann_json::nlohmann_json)

if(WIN32)
# Pass -mwindows so that no "DOS box" will open when PulseView is started.
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "-mwindows"
    OUTPUT_NAME ${DS_TITLE}
)
    # Copy language files to build output directory so the app can find them at runtime
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/lang"
            "${CMAKE_CURRENT_SOURCE_DIR}/build.windows/lang"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/build.windows/decoders/c_decoders"
        COMMENT "Stage lang/ resources into build output"
    )
endif()

set(MACOS_BUILD_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build.macOS")

#===============================================================================
#= Bundled C decoders (shipped inside the app bundle)
#-------------------------------------------------------------------------------
# Build the example SPI C decoder as a loadable module so it can be dlopen'd by
# CDecoderRegistry at startup. Installing it into <bundle>/Contents/Resources/
# share/PXTOOL/cdecoders/ (macOS) or <prefix>/share/PXTOOL/cdecoders/ (Linux)
# makes it discoverable via GetBundledCDecodeDir() = GetAppDataDir()+"/cdecoders".
add_library(spi MODULE pv/cdecoders/example_spi/spi_c_decoder.c)
set_target_properties(spi PROPERTIES
    PREFIX ""        # no "lib" prefix; produce spi.dylib / spi.so
    C_STANDARD 99
)
if(APPLE)
    set_target_properties(spi PROPERTIES SUFFIX ".dylib")
endif()

if(WIN32)
    set_target_properties(spi PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build.windows"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build.windows"
    )
elseif(APPLE)
    set_target_properties(spi PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${MACOS_BUILD_OUTPUT_DIR}"
    )
else()
    set_target_properties(spi PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build.linux"
    )
endif()

if(WIN32)
    set(C_DECODER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build.windows/decoders/c_decoders")
elseif(APPLE)
    set(C_DECODER_OUTPUT_DIR "${MACOS_BUILD_OUTPUT_DIR}/decoders/c_decoders")
else()
    set(C_DECODER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build.linux/decoders/c_decoders")
endif()

set(C_DECODERS spi_c i2c_c uart_c can_c jtag_c swd_c onewire_c i2s_c lin_c hdlc_c microwire_c mdio_c ps2_c dmx512_c nrzi_c ir_nec_c ir_rc5_c dcf77_c cec_c spdif_c usb_signalling_c 4b5b_c can_fd_c iso7816_c lpc_c dali_c c2_c graycode_c counter_c lm75_c ds1307_c ds3231_c numbers_and_state_c seven_segment_c pwm_c wiegand_c ir_sirc_c edid_c i2c_packet_c i2cdemux_c i2cfilter_c ltc26x7_c ad5593r_c adxl345_c atsha204a_c bh1750_c eeprom24xx_c flexray_c mipi_rffe_c usb_power_delivery_c iebus_c spacewire_c qspi_c sdio_c spi_dual_quad_c uart_fast_c cjtag_c mlx90614_c mpu6050_c mxc6225xu_c nunchuk_c pca9571_c rtc8564_c ssd1306_c st25dv_c tcs3472x_c tpm_tis_i2c_c tmc_c sent_c sle44xx_c pjdl_c onewire_link_c ac97_c sdcard_sd_c emmc_sd_c swim_c rvswd_c a7105_c ad5626_c ad79x0_c ade77xx_c adf435x_c adb_c afsk_c am230x_c caliper_c carrera_c xfp_c hdcp_c hdmi_scdc_c tca6408a_c tmp102_c dcc_c delta_sigma_c dsi_c em4100_c em4305_c enc28j60_c ltc242x_c max6954_c max7219_c mrf24j40_c ieee488_c ir_irmp_c ir_ltto_c ir_rc6_c ir_recoil_c eth_an_c fsi_c gpib_c guess_bitrate_c iec_c adns5020_c as5047_c avr_isp_c cc1101_c cyrf6936_c mvb_c mcs48_c one_single_wire_c ook_c opentherm_c jitter_c lfast_c maple_bus_c miller_c morse_c nes_gamepad_c nrf24l01_c nrf905_c rfm12_c ssi32_c st25r39xx_spi_c sdcard_spi_c spiflash_c spi_tpm_c tpm_tis_spi_c x2444m_c rgb_led_spi_c sda2506_c signature_c sony_md_c st7735_c st7789_c parallel_c pcfx_ctrlr_c rinnai_control_panel_c rpm_c sae_j1850_vpw_c arm_itm_c arm_tpiu_c bluetooth_h4_c boost_c crsf_c bean_c ccd_c cjtag_oscan0_c rgb_led_ws281x_c stepper_motor_c j1708_c midi_c modbus_c pan1321_c pn532_c sbus_futaba_c scs_c ufcs_c amulet_ascii_c streletz_c z80_c adat_c arm_etmv3_c aud_c avr_pdi_c jtag_avr_c jtag_ejtag_c jtag_stm32_c mipi_dsi_c pxx1_c qi_c rc_encode_c sdq_c onewire_network_c ds2408_c ds243x_c ds28ea00_c eeprom93xx_c spi_fast_c swi_c t55xx_c tdm_audio_c timing_c tlc5620_c xy2_100_c cfp_c ps2_keyboard_c ps2_mouse_c usb_packet_c usb_request_c avclan_c ethernet_c arp_c ipv4_c udp_c ook_oregon_c ook_vis_c ltar_smartdevice_c ir_ltto_decode_c sony_md_decode_c sipi_c pjon_c tpm_fifo_tis_c tm1637_c tm1638_c ltar_smartdevice_decode_c)

foreach(dec ${C_DECODERS})
    add_library(decoder_${dec} MODULE
        ${CMAKE_CURRENT_SOURCE_DIR}/libsigrokdecode/c_decoders/${dec}.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libsigrokdecode/c_decoder_api.c
    )

    target_include_directories(decoder_${dec} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libsigrokdecode
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${GLIB_INCLUDE_DIRS}
        ${Python3_INCLUDE_DIRS}
    )

    target_compile_definitions(decoder_${dec} PRIVATE
        SRD_C_DECODER_DLL
    )

    target_link_libraries(decoder_${dec}
        -lglib-2.0
    )

    set_target_properties(decoder_${dec} PROPERTIES
        OUTPUT_NAME ${dec}
        LIBRARY_OUTPUT_DIRECTORY ${C_DECODER_OUTPUT_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${C_DECODER_OUTPUT_DIR}
    )
endforeach()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE TRUE
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME ${DS_TITLE}
        MACOSX_BUNDLE_GUI_IDENTIFIER com.pxlogic.PXTOOL
        MACOSX_BUNDLE_BUNDLE_NAME ${DS_TITLE}
        MACOSX_BUNDLE_BUNDLE_VERSION ${DS_VERSION_STRING}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${DS_VERSION_STRING}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/mac_appbundle_template.plist.in
        MACOSX_BUNDLE_ICON_FILE PXTOOL.icns
        RUNTIME_OUTPUT_DIRECTORY "${MACOS_BUILD_OUTPUT_DIR}"
    )
    # `make` does not run `install`; without these POST_BUILD copies the
    # bundle would have no icon and -- more importantly -- no Python decoder
    # scripts (Resources/share/libsigrokdecode/decoders), making the
    # Decode Protocol picker show an empty list.
    set(_BUNDLE_RES_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../Resources")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # icon
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_BUNDLE_RES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/PXTOOL.icns"
            "${_BUNDLE_RES_DIR}/PXTOOL.icns"
        # Python decoders (the protocol picker reads this dir at startup)
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_BUNDLE_RES_DIR}/share/libsigrokdecode"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/libsigrokdecode/decoders"
            "${_BUNDLE_RES_DIR}/share/libsigrokdecode/decoders"
        # DSView resources (res/, demo/, lang/) used at runtime
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_BUNDLE_RES_DIR}/share/PXTOOL"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/PXTOOL/res"
            "${_BUNDLE_RES_DIR}/share/PXTOOL/res"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/PXTOOL/demo"
            "${_BUNDLE_RES_DIR}/share/PXTOOL/demo"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/lang"
            "${_BUNDLE_RES_DIR}/share/PXTOOL/lang"
        # Bundled C decoders
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_BUNDLE_RES_DIR}/share/PXTOOL/cdecoders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:spi>"
            "${_BUNDLE_RES_DIR}/share/PXTOOL/cdecoders/spi.dylib"
        COMMENT "Stage resources (icon, Python decoders, res/, demo/, lang/, cdecoders/) into app bundle"
    )
elseif(NOT WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME ${DS_TITLE}
    )
    # For Linux and other non-macOS builds, stage runtime resources used by the
    # in-tree executable. GetAppDataDir() resolves build.linux/PXTOOL to
    # ../share/PXTOOL, so C decoders must be copied under share/PXTOOL.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/lang"
            "${CMAKE_CURRENT_SOURCE_DIR}/build.linux/lang"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/share/PXTOOL/cdecoders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:spi>"
            "${CMAKE_CURRENT_SOURCE_DIR}/share/PXTOOL/cdecoders/spi.so"
        COMMENT "Stage lang/ resources into build output"
    )
endif()

if(ENABLE_COTIRE)
	include(cotire)
	cotire(${PROJECT_NAME})
endif()

if(WIN32)
    message(STATUS "Output dir: ${CMAKE_CURRENT_SOURCE_DIR}/build.windows")
    set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build.windows")
elseif(APPLE)
    message(STATUS "Output dir: ${MACOS_BUILD_OUTPUT_DIR}")
    set(EXECUTABLE_OUTPUT_PATH "${MACOS_BUILD_OUTPUT_DIR}")
else()
    message(STATUS "Output dir: ${CMAKE_CURRENT_SOURCE_DIR}/build.linux")
    set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build.linux")
endif()

#===============================================================================
#= Installation
#-------------------------------------------------------------------------------

# Install the executable.
if(APPLE)
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION .)

    # Normally resources in macOS app bundle are packed at Contents/Resources
    set(MAC_RES_PREFIX ${CMAKE_INSTALL_PREFIX}/${DS_TITLE}.app/Contents/Resources/)

    # Adding icon via add_executable / target_sources does not work, hack around this
    install(FILES PXTOOL.icns DESTINATION ${MAC_RES_PREFIX})
else()
    install(TARGETS ${PROJECT_NAME} DESTINATION bin)
endif()
install(DIRECTORY PXTOOL/res DESTINATION ${MAC_RES_PREFIX}share/PXTOOL)
install(DIRECTORY PXTOOL/demo DESTINATION ${MAC_RES_PREFIX}share/PXTOOL)
install(FILES PXTOOL/icons/logo.svg DESTINATION ${MAC_RES_PREFIX}share/PXTOOL RENAME logo.svg)
install(FILES PXTOOL/icons/logo.svg DESTINATION ${MAC_RES_PREFIX}share/icons/hicolor/scalable/apps RENAME pxtool.svg)
install(FILES PXTOOL/icons/logo.svg DESTINATION ${MAC_RES_PREFIX}share/pixmaps RENAME pxtool.svg)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")	
	install(FILES PXTOOL/PXTOOL.desktop DESTINATION /usr/share/applications RENAME pxtool.desktop)
	
	#add_compile_definitions(_DEFAULT_SOURCE)

	if(IS_DIRECTORY /usr/lib/udev/rules.d)
		install(FILES PXTOOL/DreamSourceLab.rules DESTINATION /usr/lib/udev/rules.d RENAME 60-dreamsourcelab.rules)
	elseif(IS_DIRECTORY /lib/udev/rules.d)
		install(FILES PXTOOL/DreamSourceLab.rules DESTINATION /lib/udev/rules.d RENAME 60-dreamsourcelab.rules)
	elseif(IS_DIRECTORY /etc/udev/rules.d)
		install(FILES PXTOOL/DreamSourceLab.rules DESTINATION /etc/udev/rules.d RENAME 60-dreamsourcelab.rules)
	endif()

endif()

install(FILES NEWS25 DESTINATION ${MAC_RES_PREFIX}share/PXTOOL RENAME NEWS25)
install(FILES NEWS31 DESTINATION ${MAC_RES_PREFIX}share/PXTOOL RENAME NEWS31)
install(FILES ug25.pdf DESTINATION ${MAC_RES_PREFIX}share/PXTOOL RENAME ug25.pdf)
install(FILES ug31.pdf DESTINATION ${MAC_RES_PREFIX}share/PXTOOL RENAME ug31.pdf)

install(DIRECTORY libsigrokdecode/decoders DESTINATION ${MAC_RES_PREFIX}share/libsigrokdecode)
foreach(dec ${C_DECODERS})
    install(TARGETS decoder_${dec}
        DESTINATION ${MAC_RES_PREFIX}share/libsigrokdecode/c_decoders
    )
endforeach()
install(DIRECTORY lang DESTINATION ${MAC_RES_PREFIX}share/PXTOOL)

# Bundled C decoders go alongside res/, demo/, lang/ under share/PXTOOL.
# On macOS MAC_RES_PREFIX expands to <bundle>/Contents/Resources/, on Linux it
# is empty so the destination resolves to <prefix>/share/PXTOOL/cdecoders.
install(TARGETS spi LIBRARY DESTINATION ${MAC_RES_PREFIX}share/PXTOOL/cdecoders)

#===============================================================================
#= Packaging (handled by CPack)
#-------------------------------------------------------------------------------

set(CPACK_PACKAGE_VERSION_MAJOR ${DS_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DS_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DS_VERSION_MICRO})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/PXTOOL/README)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/PXTOOL/COPYING)
set(CPACK_SOURCE_IGNORE_FILES ${CMAKE_CURRENT_BINARY_DIR} ".gitignore" ".git")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
	"${CMAKE_PROJECT_NAME}-${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_PACKAGE_CONTACT "913461865@qq.com")

# Set the final package file name.
if(WIN32)
    # This will generate: PXTOOL-1.0.0-win64.exe and PXTOOL-1.0.0-win64.zip
    set(CPACK_PACKAGE_FILE_NAME "PXTOOL-${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO}-win64")
elseif(APPLE)
    set(CPACK_PACKAGE_FILE_NAME "PXTOOL-${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO}-arm64-macOS")
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build.macOS")
else()
    set(CPACK_PACKAGE_FILE_NAME "PXTOOL-${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO}")
endif()

include(CPack)

#===============================================================================
#= Tests
#-------------------------------------------------------------------------------

if(ENABLE_TESTS)
	add_subdirectory(PXTOOL/test)
	enable_testing()
	add_test(NAME DSView-test COMMAND DSView-test)
endif(ENABLE_TESTS)
