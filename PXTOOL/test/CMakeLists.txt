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
    test_diskcachesettings.cpp
    test_logsearch.cpp
    ../pv/dock/logsearch.cpp
)

target_include_directories(DSView-test PRIVATE
    ..
    ../..
    ${QT_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(DSView-test
    ${QT_LIBRARIES}
)
