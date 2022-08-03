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
TEST=$1
FILEPATH=$2

#Delete resultFile if already exists
if [ -f "resultFile" ] ; then
    rm "resultFile"
fi

if [ -z "$FILEPATH" ];then
    echo "FILEPATH is not specified"
    echo "Taking /tmp as default FILEPATH"
    FILEPATH="/tmp"
fi

if [ "$TEST" == "--help" ];then
    echo "Available test options : \"KERNEL_CONFIG_CHECK\""
    echo "Sample command : \"sh SecurityTestTDK.sh KERNEL_CONFIG_CHECK /tmp \""

elif [ "$TEST" == "KERNEL_CONFIG_CHECK" ];then
    #Check if /proc/config.gz is present in DUT
    if [ ! -f "/proc/config.gz" ] ; then
        echo "/proc/config.gz not found"
        exit
    fi

    #Kernel config file path
    filePath="$FILEPATH/config"

    #Copy the kernel config file and extract
    if [ ! -f "$filePath" ] ; then
        cp /proc/config.gz $FILEPATH
        gunzip "$FILEPATH/config.gz"
    fi

    #Kernel configurations to be checked
    Kernel_Configs="CONFIG_DEBUG_KERNEL CONFIG_MARKERS CONFIG_DEBUG_MEMLEAK CONFIG_KPROBES CONFIG_SLUB_DEBUG CONFIG_PROFILING CONFIG_DEBUG_FS \
                    CONFIG_KPTRACE CONFIG_KALLSYMS CONFIG_LTT CONFIG_UNUSED_SYMBOLS CONFIG_TRACE_IRQFLAGS_SUPPORT CONFIG_RELAY CONFIG_MAGIC_SYSRQ \
                    CONFIG_VM_EVENT_COUNTERS CONFIGU_UNWIND_INFO CONFIG_BPA2_ALLOC_TRACE CONFIG_PRINTK CONFIG_CRASH_DUMP CONFIG_BUG CONFIG_SCSI_LOGGING \
                    CONFIG_ELF_CORE CONFIG_FULL_PANIC CONFIG_TASKSTATUS CONFIG_AUDIT CONFIG_BSD_PROCES S_ACCT CONFIG_KEXEC CONFIG_EARLY_PRINTK CONFIG_IKCONFIG \
                    CONFIG_NETFILTER_DEBUG CONFIG_MTD_UBI_DEBUG CONFIG_B43_DEBUG CONFIG_SSB_DEBUG CONFIG_FB_INTEL_DEBUG CONFIG_TRACING CONFIG_PERF_EVENTS"

    if [ -f "$filePath" ] ; then
        #Check if following kernel configurations are enabled
        for config in $Kernel_Configs
        do
            checkpattern="${config}=y"
            grep $checkpattern $filePath | grep -v "#" >> resultFile
        done
        cat resultFile | tr -d '\r\n'
        rm $filePath
    else
        echo "Extracted config file could not be found"
    fi

else
    echo "Not a valid test option"
    echo "Available test options : \"KERNEL_CONFIG_CHECK\""
fi
