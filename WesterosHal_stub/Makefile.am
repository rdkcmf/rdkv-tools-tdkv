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

AM_CXXFLAGS = ${WESTEROSHAL_FLAGS} -I${PKG_CONFIG_SYSROOT_DIR}$(includedir) \
              -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/westeros \
              -I${top_srcdir}/WesterosHal_stub/include -I${top_srcdir}/agent/include

lib_LTLIBRARIES = libwesteroshalstub.la
libwesteroshalstub_la_SOURCES = src/WesterosHalAgent.cpp
libwesteroshalstub_la_includedir = ${includedir}
libwesteroshalstub_la_LIBADD = -lwesteros_gl $(WESTEROSHAL_LIBS)
libwesteroshalstub_la_include_HEADERS = ./include/WesterosHalAgent.h


bin_PROGRAMS = WesterosHalTDKTestApp
WesterosHalTDKTestApp_SOURCES = src/WesterosTestAppSource.cpp
WesterosHalTDKTestApp_LDFLAGS= -lwesteros_gl \
			       -lwesteros_compositor
