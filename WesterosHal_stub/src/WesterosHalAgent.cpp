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

#include "WesterosHalAgent.h"
static WstGLCtx *ctx = 0;
static userData *data = 0;

/***************************************************************************
 *Function name : testmodulepre_requisites
 *Description   : testmodulepre_requisites will be used for setting the
 *                pre-requisites that are necessary for this component
 *
 *****************************************************************************/
std::string WesterosHalAgent::testmodulepre_requisites()
{
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal testmodule pre_requisites --> Entry\n");
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal testmodule pre_requisites --> Exit\n");
    return "SUCCESS";
}

/***************************************************************************
 *Function name : testmodulepost_requisites
 *Description   : testmodulepost_requisites will be used for resetting the
 *                pre-requisites that are set
 *
 *****************************************************************************/
bool WesterosHalAgent::testmodulepost_requisites()
{
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal testmodule post_requisites --> Entry\n");
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal testmodule post_requisites --> Exit\n");
    return TEST_SUCCESS;
}

/**************************************************************************
Function Name   : CreateObject
Arguments       : NULL
Description     : This function is used to create a new object of the class "DSHalAgent".
**************************************************************************/
extern "C" WesterosHalAgent* CreateObject(TcpSocketServer &ptrtcpServer)
{
        return new WesterosHalAgent(ptrtcpServer);
}

/***************************************************************************
 *Function name : initialize
 *Description   : Initialize Function will be used for registering the wrapper method
 *                with the agent so that wrapper functions will be used in the
 *                script
 *****************************************************************************/
bool WesterosHalAgent::initialize(IN const char* szVersion)
{
    DEBUG_PRINT (DEBUG_TRACE, "WesterosHal Initialization Entry\n");
    DEBUG_PRINT(DEBUG_TRACE, "Westeros init ....\n");
    ctx = WstGLInit();
    data = (userData*)calloc( 1, sizeof(userData) );
    if (ctx){
        DEBUG_PRINT(DEBUG_TRACE, "WstGLInit call success\n");
	DEBUG_PRINT (DEBUG_TRACE, "WesterosHal Initialization Exit\n");
        return "SUCCESS";
    }
    else{
        DEBUG_PRINT(DEBUG_TRACE, "WstGLInit call failed\n");
	DEBUG_PRINT (DEBUG_TRACE, "WesterosHal Initialization Exit\n");
        return "FAILURE";
    }
}

/**********************************************************************************
 *Function name : displaySizeCallback
 *Description   : Callback function to be triggered when resolution size is changed
 **********************************************************************************/
void displaySizeCallback( void *userdata, int width, int height )
{
   DEBUG_PRINT(DEBUG_TRACE, "\n\n================ Entered displaySizeCallback ===============\n");
   DEBUG_PRINT(DEBUG_TRACE, "\nReceived resolution size width = %d,  height = %d\n",width, height);

   if ( (data->wDisplayWidth != width) || (data->wDisplayHeight != height) )
   {
      DEBUG_PRINT(DEBUG_TRACE, "\nDisplay size changed: %dx%d\n", width, height);
      data->wDisplayWidth= width;
      data->wDisplayHeight= height;
   }
   DEBUG_PRINT(DEBUG_TRACE, "\n\n================ Exited displaySizeCallback ===============\n");
}

/**********************************************************************************
 *Function name : GetCallBackData
 *Description   : Returns the userData to cross verify the callback
 **********************************************************************************/
void WesterosHalAgent::WesterosHal_GetCallBackData(IN const Json::Value& req, OUT Json::Value& response)
{
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_GetCallBackData --->Entry\n");
    memset(&details[0], 0, sizeof(details));
    sprintf(details,"width=%d height=%d", data->wDisplayWidth, data->wDisplayHeight);
    response["details"] = details;
    response["result"]= "SUCCESS";
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_GetCallBackData --->Exit\n");
    return;
}


/***************************************************************************
 *Function name  : WesterosHal_CreateNativeWindow
 *Description    : This function is to create a native window
 *****************************************************************************/
void WesterosHalAgent::WesterosHal_CreateNativeWindow(IN const Json::Value& req, OUT Json::Value& response)
{
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_CreateNativeWindow --->Entry\n");
    memset(&details[0], 0, sizeof(details));
    if((&req["width"]==NULL) || (&req["height"]==NULL))
    {
        response["result"] = "FAILURE";
        response["details"] = "Invalid Height or Width of Display";
        return;
    }

    int width = req["width"].asInt();
    int height = req["height"].asInt();

    DEBUG_PRINT(DEBUG_TRACE, "\nCreate NativeWindow for %dx%d \n", width, height);
    nativeWindow = WstGLCreateNativeWindow( ctx, 0, 0, width, height );

    if (nativeWindow)
    {
        DEBUG_PRINT(DEBUG_TRACE, "\nCreation of Native Window is successfull\n");
	sprintf(details,"Creation of Native Window is successfull");
	response["details"] = details;
	response["result"]= "SUCCESS";
    }
    else
    {
	DEBUG_PRINT(DEBUG_TRACE, "\nCreation of Native Window is failedn");
        sprintf(details,"Creation of Native Window is failed");
        response["details"] = details;
        response["result"]= "FAILURE";
    }
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_CreateNativeWindow --->Exit\n");
    return;
}

/***************************************************************************
 *Function name  : WesterosHal_DestroyNativeWindow
 *Description    : This function is to destroy the native window
 *****************************************************************************/
void WesterosHalAgent::WesterosHal_DestroyNativeWindow(IN const Json::Value& req, OUT Json::Value& response)
{
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_DestroyNativeWindow --->Entry\n");
    memset(&details[0], 0, sizeof(details));

    try
    {
        WstGLDestroyNativeWindow( ctx, (void*)nativeWindow );
    }
    catch (...)
    {
	DEBUG_PRINT(DEBUG_TRACE, "\nDestruction of Native Window is failedn");
        sprintf(details,"Destruction of Native Window is failed");
        response["details"] = details;
        response["result"]= "FAILURE";
	DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_DestroyNativeWindow --->Exit\n");
	return;
    }

    DEBUG_PRINT(DEBUG_TRACE, "\nDestruction of Native Window is successfull\n");
    sprintf(details,"Destruction of Native Window is successfull");
    response["details"] = details;
    response["result"]= "SUCCESS";
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_DestroyNativeWindow --->Exit\n");
    return;
}

/***************************************************************************
 *Function name  : WesterosHal_GetDisplayInfo
 *Description    : This function returns the display info of the native window
 *****************************************************************************/
void WesterosHalAgent::WesterosHal_GetDisplayInfo(IN const Json::Value& req, OUT Json::Value& response)
{
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_GetDisplayInfo --->Entry\n");
    memset(&details[0], 0, sizeof(details));

    if( WstGLGetDisplayInfo( ctx, &displayInfo ) )
    {
	DEBUG_PRINT(DEBUG_TRACE, "\nGetDisplayInfo successfull width=%d height=%d \n", displayInfo.width, displayInfo.height);
        sprintf(details,"width=%d height=%d", displayInfo.width, displayInfo.height);
        response["details"] = details;
        response["result"]= "SUCCESS";
    }
    else
    {
        DEBUG_PRINT(DEBUG_TRACE, "\nGetDisplayInfo  is failed\n");
        sprintf(details,"GetDisplayInfo is failed");
        response["details"] = details;
        response["result"]= "FAILURE";
    }
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_GetDisplayInfo --->Exit\n");
    return;
}

/**********************************************************************************
 *Function name  : WesterosHal_GetDisplaySafeArea
 *Description    : This function returns the display safe area of the native window
 **********************************************************************************/
void WesterosHalAgent::WesterosHal_GetDisplaySafeArea(IN const Json::Value& req, OUT Json::Value& response)
{
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_GetDisplaySafeArea --->Entry\n");
    memset(&details[0], 0, sizeof(details));
    int x,y,w,h;

    if( WstGLGetDisplaySafeArea( ctx, &x, &y, &w, &h ) )
    {
        DEBUG_PRINT(DEBUG_TRACE, "\nGetDisplaySafeArea successfull x=%d y=%d w=%d h=%d \n", x, y, w, h);
        sprintf(details,"x=%d y=%d w=%d h=%d", x, y, w, h);
        response["details"] = details;
        response["result"]= "SUCCESS";
    }
    else
    {
        DEBUG_PRINT(DEBUG_TRACE, "\nGetDisplaySafeArea is failed\n");
        sprintf(details,"GetDisplaySafeArea is failed");
        response["details"] = details;
        response["result"]= "FAILURE";
    }
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_GetDisplaySafeArea --->Exit\n");
    return;
}

/********************************************************************************************************
 *Function name  : WesterosHal_AddDisplaySizeListener
 *Description    : This function registers a callback function to actively listen for display size change
 ********************************************************************************************************/
void WesterosHalAgent::WesterosHal_AddDisplaySizeListener(IN const Json::Value& req, OUT Json::Value& response)
{
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_AddDisplaySizeListener --->Entry\n");
    memset(&details[0], 0, sizeof(details));

    if( WstGLAddDisplaySizeListener( ctx, data, displaySizeCallback) )
    {
        DEBUG_PRINT(DEBUG_TRACE, "\nAddDisplaySizeListener successfull \n");
        sprintf(details,"AddDisplaySizeListener successfull");
        response["details"] = details;
        response["result"]= "SUCCESS";
    }
    else
    {
        DEBUG_PRINT(DEBUG_TRACE, "\nAddDisplaySizeListener failed\n");
        sprintf(details,"AddDisplaySizeListener failed");
        response["details"] = details;
        response["result"]= "FAILURE";
    }
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_AddDisplaySizeListener --->Exit\n");
    return;

}

/*****************************************************************
 *Function name  : WesterosHal_RemoveDisplaySizeListener
 *Description    : This function deregisters the callback function
 *****************************************************************/
void WesterosHalAgent::WesterosHal_RemoveDisplaySizeListener(IN const Json::Value& req, OUT Json::Value& response)
{
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_RemoveDisplaySizeListener --->Entry\n");
    memset(&details[0], 0, sizeof(details));

    if( WstGLRemoveDisplaySizeListener( ctx, displaySizeCallback) )
    {
        DEBUG_PRINT(DEBUG_TRACE, "\nRemoveDisplaySizeListener successfull \n");
        sprintf(details,"RemoveDisplaySizeListener successfull");
        response["details"] = details;
        response["result"]= "SUCCESS";
    }
    else
    {
        DEBUG_PRINT(DEBUG_TRACE, "\nRemoveDisplaySizeListener failed\n");
        sprintf(details,"RemoveDisplaySizeListener failed");
        response["details"] = details;
        response["result"]= "FAILURE";
    }
    DEBUG_PRINT(DEBUG_TRACE, "WesterosHal_RemoveDisplaySizeListener --->Exit\n");
    return;

}

/**************************************************************************
Function Name   : cleanup
Arguments       : NULL
Description     : This function will be used to the close things cleanly.
 **************************************************************************/
bool WesterosHalAgent::cleanup(IN const char* szVersion)
{
    DEBUG_PRINT(DEBUG_TRACE, "cleaning up\n");
    WstGLTerm( ctx );
    DEBUG_PRINT(DEBUG_TRACE,"\ncleanup ---->Exit\n");
    return TEST_SUCCESS;
}

/**************************************************************************
Function Name : DestroyObject
Arguments     : Input argument is WesterosHalAgent Object
Description   : This function will be used to destory the DSHalAgent object.
**************************************************************************/
extern "C" void DestroyObject(WesterosHalAgent *stubobj)
{
        DEBUG_PRINT(DEBUG_LOG, "Destroying WesterosHal Agent object\n");
        delete stubobj;
}
