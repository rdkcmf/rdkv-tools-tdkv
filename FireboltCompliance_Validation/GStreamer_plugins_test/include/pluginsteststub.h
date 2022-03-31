/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef __GSTREAMER_PLUGINS_TEST_STUB_H__
#define __GSTREAMER_PLUGINS_TEST_STUB_H__
#include <json/json.h>
#include "rdkteststubintf.h"
#include "rdktestagentintf.h"
#define IN
#define OUT
#include <string.h>
#include <fstream>
#include <sys/wait.h>
#include <stdlib.h>
#include <jsonrpccpp/server/connectors/tcpsocketserver.h>
#include <unistd.h>

#define LOGPATH "LOGPATH_INFO"
#define SUITE_STATUS "SUITE_STATUS"
#define MAIN_SUITE "ExecuteSuite.sh"
using namespace std;

class RDKTestAgent;
class GStreamer_plugins_test : public RDKTestStubInterface  , public AbstractServer<GStreamer_plugins_test>
{
	public:
                GStreamer_plugins_test(TcpSocketServer &ptrRpcServer) : AbstractServer <GStreamer_plugins_test>(ptrRpcServer)
                {
                    this->bindAndAddMethod(Procedure("TestMgr_Gstreamer_Test_Execute", PARAMS_BY_NAME, JSON_STRING,"GStreamer_plugins_type",JSON_STRING,"Display_option",JSON_STRING,NULL), &GStreamer_plugins_test::GStreamer_plugins_test_Execute);
                }
		std::string testmodulepre_requisites();
                bool testmodulepost_requisites();
		bool initialize(IN const char* szVersion);
		bool cleanup(IN const char* szVersion);
		void GStreamer_plugins_test_Execute(IN const Json::Value& req, OUT Json::Value& response);
	private:
		string GetLogPath(string);
		string StatusOfSuite(string);
};

#endif //__GSTREAMER_PLUGINS_TEST_STUB_H__
