/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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

#ifndef __WESTEROSHAL_STUB_H__
#define __WESTEROSHAL_STUB_H__

#include <json/json.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "rdkteststubintf.h"
#include "rdktestagentintf.h"
#include "westeros-gl.h"

#include <jsonrpccpp/server/connectors/tcpsocketserver.h>

#define IN
#define OUT

#define TEST_SUCCESS true
#define TEST_FAILURE false

using namespace std;

typedef struct _UserData
{
  int wDisplayWidth;
  int wDisplayHeight;
} userData;

void *nativeWindow;
WstGLDisplayInfo displayInfo;
char details[100];

class RDKTestAgent;
class WesterosHalAgent : public RDKTestStubInterface , public AbstractServer<WesterosHalAgent>
{
        public:
                //Constructor
                WesterosHalAgent(TcpSocketServer &ptrRpcServer) : AbstractServer <WesterosHalAgent>(ptrRpcServer)
                {
                    this->bindAndAddMethod(Procedure("TestMgr_WesterosHal_CreateNativeWindow", PARAMS_BY_NAME, JSON_STRING, "width", JSON_INTEGER, "height",  JSON_INTEGER, NULL), &WesterosHalAgent::WesterosHal_CreateNativeWindow);
		    this->bindAndAddMethod(Procedure("TestMgr_WesterosHal_DestroyNativeWindow",PARAMS_BY_NAME, JSON_STRING, NULL), &WesterosHalAgent::WesterosHal_DestroyNativeWindow);
		    this->bindAndAddMethod(Procedure("TestMgr_WesterosHal_GetDisplayInfo",PARAMS_BY_NAME, JSON_STRING, NULL), &WesterosHalAgent::WesterosHal_GetDisplayInfo);
		    this->bindAndAddMethod(Procedure("TestMgr_WesterosHal_GetCallBackData",PARAMS_BY_NAME, JSON_STRING, NULL), &WesterosHalAgent::WesterosHal_GetCallBackData);
		    this->bindAndAddMethod(Procedure("TestMgr_WesterosHal_GetDisplaySafeArea",PARAMS_BY_NAME, JSON_STRING, NULL), &WesterosHalAgent::WesterosHal_GetDisplaySafeArea);
		    this->bindAndAddMethod(Procedure("TestMgr_WesterosHal_AddDisplaySizeListener",PARAMS_BY_NAME, JSON_STRING, NULL), &WesterosHalAgent::WesterosHal_AddDisplaySizeListener);
		    this->bindAndAddMethod(Procedure("TestMgr_WesterosHal_RemoveDisplaySizeListener",PARAMS_BY_NAME, JSON_STRING, NULL), &WesterosHalAgent::WesterosHal_RemoveDisplaySizeListener);
                }


                //Inherited functions
                bool initialize(IN const char* szVersion);
                bool cleanup(const char* szVersion);
                std::string testmodulepre_requisites();
                bool testmodulepost_requisites();

                //Stub functions
                void WesterosHal_CreateNativeWindow(IN const Json::Value& req, OUT Json::Value& response);
		void WesterosHal_DestroyNativeWindow(IN const Json::Value& req, OUT Json::Value& response);
		void WesterosHal_GetDisplayInfo(IN const Json::Value& req, OUT Json::Value& response);
		void WesterosHal_GetCallBackData(IN const Json::Value& req, OUT Json::Value& response);
		void WesterosHal_GetDisplaySafeArea(IN const Json::Value& req, OUT Json::Value& response);
		void WesterosHal_AddDisplaySizeListener(IN const Json::Value& req, OUT Json::Value& response);
                void WesterosHal_RemoveDisplaySizeListener(IN const Json::Value& req, OUT Json::Value& response);


};
#endif //__WESTEROSHAL_STUB_H__


