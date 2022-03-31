##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2022 RDK Management
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
#Setting default values to Environment variables
EXEC_STRING="NULL"
SUMMARY_LOG_NAME=TestSummary.log
GSTREAMER_BIN_PATH=$OPENSOURCETEST_PATH/gstreamer
GSTREAMER_LOG_PATH=$OPENSOURCETEST_PATH/logs/gstreamer
GSTREAMER_GOOD_BIN_PATH=$OPENSOURCETEST_PATH/gst-plugin-good
GSTREAMER_GOOD_LOG_PATH=$OPENSOURCETEST_PATH/logs/gst-plugin-good
GSTREAMER_BASE_BIN_PATH=$OPENSOURCETEST_PATH/gst-plugin-base
GSTREAMER_BASE_LOG_PATH=$OPENSOURCETEST_PATH/logs/gst-plugin-base
EXECUTER_PATH=$OPENSOURCETEST_PATH
FILES_TO_IGNORE=open,applications,collection,*.sh,*.log,*.txt,all,new,*.png,silly,open,*.pdf,resources,auth-test,dns,get,getbug,proxy-test,pull-api,range-test,simple-httpd,simple-proxy,lt-gstpipeline
EXECUTER_PATH=$OPENSOURCETEST_PATH
LOG_NAME=$SUMMARY_LOG_NAME
echo $LOG_NAME
GSTREAMER="gstreamer"

#Below environment can be used when a plugin needs to be ignored from the testing.
export GST_STATE_IGNORE_ELEMENTS=
export GST_PLUGIN_LOADING_WHITELIST=
#Listing the usage of Suite Executor script
usage() {

echo "Usage: `basename $0` [-c Component name] [-h help]"
echo " -c     Specify the name of the component."
echo "Example:Enter gstreamer for gstreamer, gst_plugin_base for gstreamer-plugins-base or gst_plugin_good for gstreamer-plugins-good test suite execution"
}
while getopts ":hp:o:c:" opt; do
    case $opt in
        h)
            usage
            exit
            ;;
        c)
            COMPONENT_NAME=$OPTARG
            echo $COMPONENT_NAME
            ;;
        o)
            OPTIONS=$OPTARG
            echo $OPTIONS
            ;;
        \?)
            usage
            exit
            ;;
        :)
           echo "Option -$OPTARG requires an argument." >&2
           echo "   try 'ExecuteSuite.sh -h' for more information"
           exit 1
           ;;
    esac
done

if [ -z "$1" ]; then
  usage
  exit 1
fi

if [ -z "$COMPONENT_NAME" ]; then
  echo "-c 'COMPONENT NAME' is a required argument" >&2
  exit 1
fi

#Checking for component name and setting the respective environment variable
case $COMPONENT_NAME in
     gstreamer)
         BIN_PATH=$GSTREAMER_BIN_PATH
         SEARCH_KEY="Checks:"
         OPTIONS="NULL"
         LOG_PATH=$GSTREAMER_LOG_PATH
         ;;
     gst_plugin_base)
         BIN_PATH=$GSTREAMER_BASE_BIN_PATH
         SEARCH_KEY="Checks:"
         OPTIONS="NULL"
         LOG_PATH=$GSTREAMER_BASE_LOG_PATH
         ;;
     gst_plugin_good)
         BIN_PATH=$GSTREAMER_GOOD_BIN_PATH
         SEARCH_KEY="Checks:"
         OPTIONS="NULL"
         LOG_PATH=$GSTREAMER_GOOD_LOG_PATH
         ;;
     gst_plugin_bad)
         BIN_PATH=$GSTREAMER_BAD_BIN_PATH
         SEARCH_KEY="Checks:"
         OPTIONS="NULL"
         LOG_PATH=$GSTREAMER_BAD_LOG_PATH
         ;;
     gst_plugin_custom)
         BIN_PATH=$GSTREAMER_PLUGIN_CUSTOM_BIN_PATH
         SEARCH_KEY="Checks:"
         OPTIONS="NULL"
         LOG_PATH=$GSTREAMER_PLUGIN_CUSTOM_LOG_PATH
         ;;
       *)
         echo "At this moment gstreamer,gstreamer base plugin and gstreamer good plugin tests can run "
         exit
         ;;
esac

generate_component_exec_summary()
{
    COMPONENT_NAME=$1
    TOTAL_SUITE_COUNT=$2
    EXECUTE_SUCCESS_COUNT=$3
    EXECUTE_FAILURE_COUNT=$4
    TEST_SUMMARY_LOG=$LOG_PATH/$LOG_NAME

    COMP_LOG="$LOG_PATH/ComponentSummary.log"
    TEMP_COMP_LOG="$LOG_PATH/TempComponentSummary.log"
    touch $COMP_LOG
    touch $TEMP_COMP_LOG

    echo -e "\n------------------------------------------------------" >  $COMP_LOG
    echo -e "TEST EXECUTION SUMMARY" >> $COMP_LOG
    echo -e   "------------------------------------------------------" >> $COMP_LOG
    echo -e "Component Name        : $COMPONENT_NAME\n"        >> $COMP_LOG
    echo -e "Total No.of Suite(s)  : $TOTAL_SUITE_COUNT"       >> $COMP_LOG
    echo -e "Executed Suite(s)     : $EXECUTE_SUCCESS_COUNT"   >> $COMP_LOG
    echo -e "Not Executed Suite(s) : $EXECUTE_FAILURE_COUNT\n" >> $COMP_LOG

    if [[ -f $TEST_SUMMARY_LOG ]]; then
        SUITE_PASS_COUNT=$(awk '/100%: Checks:/{print $0}' $TEST_SUMMARY_LOG | cut -d ";" -f 1 | cut -d ":" -f 2 | wc -l)
        SUITE_PASS_LIST=$(awk '/100%: Checks:/{print $0}'  $TEST_SUMMARY_LOG | cut -d ";" -f 1 | cut -d ":" -f 2 | tr "\n" " ")
        SUITE_FAIL_COUNT=$(awk '/Failures: [1-9].*|Errors: [1-9].*/{print $0}' $TEST_SUMMARY_LOG | cut -d ";" -f 1 | cut -d ":" -f 2 | wc -l)
        SUITE_FAIL_LIST=$(awk '/Failures: [1-9].*|Errors: [1-9].*/{print $0}' $TEST_SUMMARY_LOG | cut -d ";" -f 1 | cut -d ":" -f 2 | tr "\n" " ")
        echo -e "No.of Passed Suite(s) : $SUITE_PASS_COUNT"    >> $COMP_LOG
        echo -e "No.of Failed Suite(s) : $SUITE_FAIL_COUNT\n"  >> $COMP_LOG
        echo -e "Passed Suite List     : [ $SUITE_PASS_LIST ]" >> $COMP_LOG
        echo -e "Failed Suite List     : [ $SUITE_FAIL_LIST ]" >> $COMP_LOG

        cat $COMP_LOG > $TEMP_COMP_LOG

        SUITE_FAIL_DETAILS=$(awk '/Failures: [1-9].*|Errors: [1-9].*/{print $0}' $TEST_SUMMARY_LOG)
        if [[ ! -z $SUITE_FAIL_DETAILS && $SUITE_FAIL_DETAILS != "  " ]]
        then
            echo -e "\nFailed Test Suite Details:" >> $COMP_LOG
            echo -e "****************************\n" >> $COMP_LOG
            ITER=1
            while read -r failed_suite_info; do
                test_suite_name=$(echo $failed_suite_info | cut -d ";" -f 1 | cut -d ":" -f 2)
                test_suite_file=$(echo $failed_suite_info | cut -d ";" -f 3)
                TC_FAILURE_COUNT=$(cat $test_suite_file | grep "Checks:" | awk -F'Failures: '  '{print $2}' | cut -d "," -f1 | awk '{s+=$1} END {printf "%.0f", s}')
                TC_ERROR_COUNT=$(cat $test_suite_file | grep "Checks:" | awk -F'Errors: '  '{print $2}' | cut -d "," -f1 | awk '{s+=$1} END {printf "%.0f", s}')
                TOTAL_TC_FAIL_ERROR_COUNT=$(($TC_FAILURE_COUNT+$TC_ERROR_COUNT))
                FAILURE_TESTS=$(cat $test_suite_file | grep -A$TOTAL_TC_FAIL_ERROR_COUNT "Checks:" | grep "\.c" |awk -F: '{print $(NF-2)}' | uniq | tr "\n" " ")
                echo -e "-----------------------------------" >>  $COMP_LOG
                echo -e "$ITER) Test Suite Name : "$test_suite_name  >> $COMP_LOG
                echo -e "-----------------------------------" >>  $COMP_LOG
                echo -e "List of Failed Test cases: [ $FAILURE_TESTS ]" >> $COMP_LOG
                cat $test_suite_file >> $COMP_LOG
                echo -e "\n\n"       >> $COMP_LOG
                ITER=$(( $ITER + 1 ))
            done <<< "$SUITE_FAIL_DETAILS "
        fi
    else
        cat $COMP_LOG > $TEMP_COMP_LOG
    fi
}

#Depends on the argument ,Execute the respective test suite.
Execute(){
   BIN_PATH=$1
   NEXT_LINE='
'
   LOG_NAME=$3
   COMP_NAME=$2
   EXEC_OPTION=$4
   KEY_WORD=$5
   LOG_PATH=$6
   echo "EXECUTER_PATH" $$EXECUTER_PATH

   #Removing the old logs and Summary log
   rm -f $EXECUTER_PATH/LOGPATH_INFO
   rm -f $LOG_PATH/log_*
   rm -f $LOG_PATH/$LOG_NAME
   rm -f $LOG_PATH/TEMPFILE*
   rm -f $LOG_PATH/TEMP_FILES
   rm -f $EXECUTER_PATH/SUITE_STATUS

   #Navigate in to mentioned path
   cd $BIN_PATH
   if [ $? != 0 ]; then
        echo $BIN_PATH is not found
        echo $BIN_PATH is not found >> $EXECUTER_PATH/SUITE_STATUS
        echo $BIN_PATH "is not found" > $EXECUTER_PATH/LOGPATH_INFO
        exit 1
   fi
   echo "Binary path:"$BIN_PATH
   echo "Component Name:"$COMPONENT_NAME
   #Creating the summary log and changing the log file permissions
   #Creating the summary log and changing the log file permissions
   mkdir -p $LOG_PATH
   touch $LOG_PATH/$LOG_NAME
   chmod 777 $LOG_PATH/$LOG_NAME
   IGNORE_LIST=`echo $FILES_TO_IGNORE | tr "," " "`
   echo "Ignore File List :"$IGNORE_LIST
   #Finding the total executables counts
   FILE_LIST=$(ls)
   echo "FILE_LIST:"$FILE_LIST

   #Ignoring the list of files from the execution list
   for FILE in $FILE_LIST
   do
        echo $FILE$NEXT_LINE >> $LOG_PATH/TEMP_FILES
   done
   for IGNORE_FILE in $IGNORE_LIST
   do
        echo "IGNOREFILE:"$IGNORE_FILE
        sed -i "/$IGNORE_FILE/d" $LOG_PATH/TEMP_FILES
   done
   FILE_LIST=`cat $LOG_PATH/TEMP_FILES`
   echo "FILE_LIST:"$FILE_LIST

   TOTAL_FILE_COUNT=`cat $LOG_PATH/TEMP_FILES |wc -l`
   echo "Total file count :"$TOTAL_FILE_COUNT
   echo "Total Test suite available :"$TOTAL_FILE_COUNT >> $LOG_PATH/$LOG_NAME
   if [ $TOTAL_FILE_COUNT == 0 ]; then
        echo "No binaries are availble to execute"
        echo "No binaries are availble to execute" > $EXECUTER_PATH/SUITE_STATUS
        echo "No binaries are availble to execute" > $EXECUTER_PATH/LOGPATH_INFO
        exit 1
   fi
   EXECUTE_SUCCESS_COUNTER=0
   EXECUTE_FAILURE_COUNTER=0
   #Test Execution
   echo "FILE_LIST:"$FILE_LIST
   for ELEMENT in $FILE_LIST
   do
          echo "Test Suite Name :"$ELEMENT
          echo "Test Suite Name :"$ELEMENT";" > $LOG_PATH/TEMPFILE1
          ./$ELEMENT &> $LOG_PATH/log_$ELEMENT

          #Collecting the log details and store in to Summary_Log
          echo "Finding the execution status from log with the keyword : "$KEY_WORD
          cat  $LOG_PATH/log_$ELEMENT
          cat  $LOG_PATH/log_$ELEMENT|grep $KEY_WORD > $LOG_PATH/TEMPFILE2
          #Checking the results of each suite execution
          EXECUTION_CHECK=`cat  $LOG_PATH/log_$ELEMENT|grep -c $KEY_WORD`
          if [ $EXECUTION_CHECK -gt 0 ]; then
               EXECUTE_SUCCESS_COUNTER=$((EXECUTE_SUCCESS_COUNTER+1))
               echo "Number of TestSuite Executed :"$EXECUTE_SUCCESS_COUNTER
          else
               EXECUTE_FAILURE_COUNTER=$((EXECUTE_FAILURE_COUNTER+1))
               echo "Number of TestSuite Not Executed :"$EXECUTE_FAILURE_COUNTER
          fi
          touch $EXECUTER_PATH/SUITE_STATUS
          echo "TotalSuite:"$TOTAL_FILE_COUNT" TestSuiteExecuted:"$EXECUTE_SUCCESS_COUNTER" SuiteNotExecuted:"$EXECUTE_FAILURE_COUNTER > $EXECUTER_PATH/SUITE_STATUS
          #Exporting log path in to temp file
          echo ";"$LOG_PATH"/log_"$ELEMENT >$LOG_PATH/TEMPFILE3

          #Exporting the Summary details in to TestSummary.log
          cat $LOG_PATH/TEMPFILE1 $LOG_PATH/TEMPFILE2 $LOG_PATH/TEMPFILE3|tr '\n' " " >> $LOG_PATH/$LOG_NAME
          echo $NEXT_LINE >> $LOG_PATH/$LOG_NAME
          #Removing the Temp Files
          rm -f $LOG_PATH/TEMPFILE*
    done

    echo "Generating component execution summary...."
    generate_component_exec_summary $COMPONENT_NAME $TOTAL_FILE_COUNT $EXECUTE_SUCCESS_COUNTER $EXECUTE_FAILURE_COUNTER
    mv $LOG_PATH/$LOG_NAME $LOG_PATH/temp_summary.log
    mv $COMP_LOG $LOG_PATH/$LOG_NAME
    #LOG_NAME=temp_summary.log
    cat $TEMP_COMP_LOG
    rm -rf $TEMP_COMP_LOG &2>/dev/null

    #Exporting Summary log path and file name to stub
    touch $EXECUTER_PATH/LOGPATH_INFO
    echo "Generating Log Path Info......"
    echo $LOG_PATH"/"$LOG_NAME >> $EXECUTER_PATH/LOGPATH_INFO
}

#Executing the testsuite
Execute $BIN_PATH $COMPONENT_NAME $LOG_NAME $OPTIONS $SEARCH_KEY $LOG_PATH
