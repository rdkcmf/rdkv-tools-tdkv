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

#include "pluginsteststub.h"

/***************************************************************************************************
 *Function name	: initialize
 *Descrption	: Initialize Function will be used for registering the wrapper method 
 * 	 	  with the agent so that wrapper functions will be used in the 
 *  		  script
 ***************************************************************************************************/ 
bool GStreamer_plugins_test::initialize(IN const char* szVersion)
{
	DEBUG_PRINT(DEBUG_TRACE,"\nGStreamer_plugins_test Initialize");
	return true;
}

/***************************************************************************
 *Function name : testmodulepre_requisites
 *Descrption    : testmodulepre_requisites will  be used for setting the
 *                pre-requisites that are necessary for this component
 *
 *****************************************************************************/
std::string GStreamer_plugins_test::testmodulepre_requisites()
{
        return "SUCCESS";
}

/***************************************************************************
 *Function name : testmodulepost_requisites
 *Descrption    : testmodulepost_requisites will be used for resetting the
 *                pre-requisites that are set
 *
 *****************************************************************************/
bool GStreamer_plugins_test::testmodulepost_requisites()
{
        return true;
}

/*******************************************************************************************************
 *Function name	: GStreamer_plugins_test_Execute
 *Descrption	: GStreamer_plugins_test_Execute wrapper function will be used to execute opensource test suites
 *******************************************************************************************************/ 
void GStreamer_plugins_test::GStreamer_plugins_test_Execute(IN const Json::Value& req, OUT Json::Value& response)
{
	DEBUG_PRINT(DEBUG_TRACE,"\nEntering GStreamer_plugins_test_Execute function");
	if (getenv ("OPENSOURCETEST_PATH")!=NULL)
	{
		/* Declaring the variables */
		string testenvPath,executerPath, logger_path,get_status;
		int iTestAppStatus;

		testenvPath = getenv ("OPENSOURCETEST_PATH");
		if (testenvPath.empty()==0)
		{
			executerPath.append(testenvPath);
			executerPath.append("/");
			executerPath.append(MAIN_SUITE);
			
			/*Creating the process for Suite execution*/
			char *component_option=(char*)req["GStreamer_plugins_type"].asCString();
			DEBUG_PRINT(DEBUG_LOG,"\nGstreamer component option %s",component_option);
			DEBUG_PRINT(DEBUG_LOG,"\nexecuterPath %s",executerPath.c_str());

			pid_t idChild = vfork();
			if(idChild == 0)
			{
				/*getting the display options from the test framework*/
				char *display_selection=(char*)req["Display_option"].asCString();
				if(strcmp(component_option,"gstreamer") == 0)
				{
					/*Executing the suite executer script to invoke all Gstreamer test suites*/
					DEBUG_PRINT(DEBUG_TRACE,"\n Invoke ExecuteSuite script to execute the Gstreamer tests");
					execlp(executerPath.c_str(),"ExecuteSuite.sh","-c","gstreamer",NULL);
				}
				else if(strcmp(component_option,"gst-plugin-base") == 0)
                                {
                                        /*Executing the suite executer script to invoke all Gst-Plugin-base test suites*/
                                        DEBUG_PRINT(DEBUG_TRACE,"\n Invoke ExecuteSuite script to execute the Gst-plugin-base tests");
                                        execlp(executerPath.c_str(),"ExecuteSuite.sh","-c","gst_plugin_base",NULL);
                                }
				else if(strcmp(component_option,"gst-plugin-custom") == 0)
				{
					/*Executing the suite executer script to invoke all Gst-Plugin-base test suites*/
					DEBUG_PRINT(DEBUG_TRACE,"\n Invoke ExecuteSuite script to execute the Gst-plugin-custom tests");
					execlp(executerPath.c_str(),"ExecuteSuite.sh","-c","gst_plugin_custom",NULL);
				}
				else if(strcmp(component_option,"gst-plugin-good") == 0)
				{
					/*Executing the suite executer script to invoke all Gst-Plugin-good test suites*/
					DEBUG_PRINT(DEBUG_TRACE,"\n Invoke ExecuteSuite script to execute the Gst-plugin-good tests");
					execlp(executerPath.c_str(),"ExecuteSuite.sh","-c","gst_plugin_good",NULL);
				}
				else if(strcmp(component_option,"gst-plugin-bad") == 0)
                                {
                                        /*Executing the suite executer script to invoke all Gst-Plugin-good test suites*/
                                        DEBUG_PRINT(DEBUG_TRACE,"\n Invoke ExecuteSuite script to execute the Gst-plugin-bad tests");
                                        execlp(executerPath.c_str(),"ExecuteSuite.sh","-c","gst_plugin_bad",NULL);
                                }
				else 
				{
					DEBUG_PRINT(DEBUG_ERROR,"\n Not an Valid Test suite option");
					response["result"]="Not a valid test suite option for execution ";
				}
			}
			else if(idChild <0)
			{
				/* Filling the response back with error message in case of fork failure*/
				DEBUG_PRINT(DEBUG_ERROR,"\nfailed fork");
				response["result"]="Test Suite Execution failed";
			}
			else
			{
				/* Wait for the Suite execution to get over */  
				waitpid(idChild,&iTestAppStatus,0);

				{
					/* Get the execution status */
					get_status = StatusOfSuite(testenvPath);
					DEBUG_PRINT(DEBUG_LOG,"\n Getting the Execution status : %s\n",get_status.c_str());
					if(get_status.empty() == 0)
					{
						response["details"]= get_status;
						response["result"]="Test Suite Executed";
					}
					else
					{
						response["result"]="Test Suite Execution Failed";
						response["details"]= "NO SUITE_STATUS file available to get the information about exectution status ";
					}
				}

				/* Get the Summary file path */
				logger_path = GetLogPath(testenvPath);
				DEBUG_PRINT(DEBUG_LOG,"\n Getting the Test Summary Log path \n");
				DEBUG_PRINT(DEBUG_LOG,"\n Test Summary Log path :%s\n",logger_path.c_str());
				if (logger_path.empty() == 0)
				{
					response["log-path"]=logger_path;
				}
				else
				{
					response["details"]= "NO LOGPATH_INFO file available to get the information about log path";
				} 
			}

		} 
		else
		{
			DEBUG_PRINT(DEBUG_ERROR,"\n Not able to read the environment path\n");
			response["details"]= "\n  Not able to read the environment path \n";
			response["result"]="Test Suite Execution Failed";
		}
	}
	else
	{
		DEBUG_PRINT(DEBUG_ERROR,"\n OPENSOURCETEST_PATH export is not set. Kindly set the RDKTDK_PATH path\n");
		response["details"]= "\n OPENSOURCETEST_PATH export is not set. Kindly set the RDKTDK_PATH path\n";
		response["result"]="Test Suite Execution Failed";
	}
	DEBUG_PRINT(DEBUG_TRACE,"\nExiting GStreamer_plugins_test_Execute function");

	return ;
}
/******************************************************************************************************************************
 *Function name	: GetLogPath
 *Descrption	: This function will help in reading the LOGPATH_INFO file and give the details of the executed test suite log
 *       returns a string value
 *****************************************************************************************************************************/ 
string GStreamer_plugins_test:: GetLogPath(string envPath)
{
	DEBUG_PRINT(DEBUG_TRACE,"\n Entering GetLogPath function \n");
	string logpathobj,logPath;
	printf ("%s",envPath.c_str());
	logPath.append(envPath);
	logPath.append("/");
	logPath.append(LOGPATH);

	ifstream input(logPath.c_str());
	if(input.is_open())
	{
		while(!input.eof())
		{
			DEBUG_PRINT(DEBUG_LOG,"\n Reading the status details from LOGPATH_INFO \n");
			getline(input,logpathobj);
			input.close();
			return logpathobj;
		}
	}
	else
	{
		DEBUG_PRINT(DEBUG_ERROR,"\n Not able to open LOGPATH_INFO file\n");
	}
	DEBUG_PRINT(DEBUG_TRACE,"\n Exiting GetLogPath function \n");
	return NULL;
}

/******************************************************************************************************************************
 *Function name	: StatusOfSuite
 *Descrption	: This function will help in reading the SUITESTATUS file and give the details of the executed test suite log
 *       returns a string value
 *****************************************************************************************************************************/ 
string GStreamer_plugins_test:: StatusOfSuite(string envPath1)
{
	DEBUG_PRINT(DEBUG_TRACE,"\n Entering StatusOfSuite function \n");

	string logstatusobj,statusPath;
	printf ("%s",envPath1.c_str());
	statusPath.append(envPath1);
	statusPath.append("/");
	statusPath.append(SUITE_STATUS);
	ifstream input(statusPath.c_str());
	if(input.is_open())
	{
		while(!input.eof())
		{
			DEBUG_PRINT(DEBUG_LOG,"\n Reading the status details from LOGPATH_INFO \n");
			getline(input,logstatusobj);
			input.close();
			return logstatusobj;
		}	
	}
	else
	{
		DEBUG_PRINT(DEBUG_ERROR,"\n Not able to open SUITE_STATUS file\n");
	}

	DEBUG_PRINT(DEBUG_TRACE,"\n Exiting StatusOfSuite function \n");
	return NULL;
}

/**************************************************************************
 * Function Name : CreateObject
 * Description   : this function will be used to create a new object
 *      for the class GStreamer_plugins_test
 **************************************************************************/
extern "C" GStreamer_plugins_test* CreateObject(TcpSocketServer &ptrtcpServer)
{
	DEBUG_PRINT(DEBUG_LOG,"\n Creating the Open source test stub object \n");
	return new GStreamer_plugins_test(ptrtcpServer);
}

/**************************************************************************
 * Function Name : cleanup
 * Description   : This function will do the clean up.
 *
 **************************************************************************/
bool GStreamer_plugins_test::cleanup(IN const char* szVersion)
{
	DEBUG_PRINT(DEBUG_LOG,"\n GStreamer_plugins_test shutting down ");
	return true;
}

/**************************************************************************
 * Function Name : DestroyObject
 * Description   : This function will be used to destory the object. 
 *
 **************************************************************************/
extern "C" void DestroyObject(GStreamer_plugins_test *stubobj)
{
	DEBUG_PRINT(DEBUG_LOG,"\n Destroying Opensourcetest stub object");
	delete stubobj;


}
