##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
ACLOCAL_AMFLAGS = -Im4 --install

AM_CPPFLAGS = -g -Wall -std=c++0x -DGCC4_XXX \
              -fPIC ${COPTS}

AM_CXXFLAGS =  ${PWRMGRHAL_FLAGS} -I${PKG_CONFIG_SYSROOT_DIR}$(includedir) \
              -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs-hal \
              -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs/mfr \
              -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmbus \
              -I${top_srcdir}/PowerMgrHal_stub/include -I${top_srcdir}/agent/include

AM_CFLAGS =  ${PWRMGRHAL_FLAGS} -I${PKG_CONFIG_SYSROOT_DIR}$(includedir) \
            -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs/mfr \
            -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs-hal \
            -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmbus


lib_LTLIBRARIES = libpwrmgrhalstub.la
libpwrmgrhalstub_la_SOURCES = src/iarmmgr_source/power/therm_mon.c src/PowerMgrHalAgent.cpp
libpwrmgrhalstub_la_includedir = ${includedir}
libpwrmgrhalstub_la_LIBADD = -liarmmgrs-power-hal $(PWRMGRHAL_LIBS)
libpwrmgrhalstub_la_include_HEADERS = ${top_srcdir}/PowerMgrHal_stub/include/PowerMgrHalAgent.h


