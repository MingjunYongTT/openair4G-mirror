#!/bin/bash
################################################################################
# Eurecom OpenAirInterface core network
# Copyright(c) 1999 - 2014 Eurecom
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#
# The full GNU General Public License is included in this distribution in
# the file called "COPYING".
#
# Contact Information
# Openair Admin: openair_admin@eurecom.fr
# Openair Tech : openair_tech@eurecom.fr
# Forums       : http://forums.eurecom.fsr/openairinterface
# Address      : EURECOM,
#                Campus SophiaTech,
#                450 Route des Chappes,
#                CS 50193
#                06904 Biot Sophia Antipolis cedex,
#                FRANCE
################################################################################
# file start_enb.bash
# brief
# author Lionel Gauthier
# company Eurecom
# email: lionel.gauthier@eurecom.fr
###########################################
# INPUT OF THIS SCRIPT:
# THE DIRECTORY WHERE ARE LOCATED THE CONFIGURATION FILES
#########################################
# This script start  ENB+UE (all in one executable, on one host)
# Depending on configuration files, it can be instanciated a virtual switch 
# setting or a VLAN setting.
# MME+SP-GW executable have to be launched on the same host by your own (start_epc.bash) before this script is invoked.
#
###########################################################################################################################
#                                    VIRTUAL SWITCH SETTING
###########################################################################################################################
#
#                                                                           hss.eur
#                                                                             |
#        +-----------+          +------+              +-----------+           v   +----------+
#        |  eNB      +------+   |  ovs | VLAN 1+------+    MME    +----+      +---+   HSS    |
#        |           |cpenb0+------------------+cpmme0|           |    +------+   |          |
#        |           +------+   |bridge|       +------+           +----+      +---+          |
#        |           |upenb0+-------+  |              |           |               +----------+
#        +-----------+------+   |   |  |              +-----------+
#                               +---|--+                    |                   router.eur
#                                   |                 +-----------+              |   +--------------+
#                                   |                 |  S+P-GW   |              v   |   ROUTER     |
#                                   |  VLAN2   +------+           +-------+     +----+              +----+
#                                   +----------+upsgw0|           |sgi    +-...-+    |              |    +---...Internet
#                                              +------+           +-------+     +----+              +----+
#                                                     |           |      11 VLANS    |              |
#                                                     +-----------+   ids=[5..15]    +--------------+
#
#
###########################################################################################################################
#                                    VLAN SETTING
###########################################################################################################################
#                                                                           hss.eur
#                                                                             |
#        +-----------+                                +-----------+           v   +----------+
#        |  eNB      +------+            VLAN 1+------+    MME    +----+      +---+   HSS    |
#        |           |ethx.1+------------------+ethy.1|           |    +------+   |          |
#        |           +------+                  +------+           +----+      +---+          |
#        |           |ethx.2+-------+                 |           |               +----------+
#        |           +------+       |                 +-+-------+-+
#        |           |              |                   | s11mme|    
#        |           |              |                   +---+---+    
#        |           |              |             (optional)|   VLAN 3
#        +-----------+              |                   +---+---+    
#                                   |                   | s11sgw|            router.eur
#                                   |                 +-+-------+-+              |   +--------------+
#                                   |                 |  S+P-GW   |              v   |   ROUTER     |
#                                   |  VLAN2   +------+           +-------+     +----+              +----+
#                                   +----------+ethy.2|           |sgi    +-...-+    |              |    +---...Internet
#                                              +------+           +-------+     +----+              +----+
#                                                     |           |      11 VLANS    |              |
#                                                     +-----------+   ids=[5..15]    +--------------+

###########################################################
# Parameters
###########################################################
declare MAKE_LTE_ACCESS_STRATUM_TARGET="oaisim ENABLE_ITTI=1 USE_MME=R10 LINK_PDCP_TO_GTPV1U=1 NAS=1 Rel10=1 ASN_DEBUG=1 EMIT_ASN_DEBUG=1"
declare MAKE_IP_DRIVER_TARGET="ue_ip.ko"
declare IP_DRIVER_NAME="ue_ip"
declare LTEIF="oip1"
declare UE_IPv4="10.0.0.8"
declare UE_IPv6="2001:1::8"
declare UE_IPv6_CIDR=$UE_IPv6"/64"
declare UE_IPv4_CIDR=$UE_IPv4"/24"


###########################################################
THIS_SCRIPT_PATH=$(dirname $(readlink -f $0))
source $THIS_SCRIPT_PATH/utils.bash
###########################################################
if [ $# -eq 1 ]; then
    declare -x CONFIG_FILE_DIR=$1
    if [ ! -d $CONFIG_FILE_DIR ]; then
        echo_error "ERROR while invoking this script, as first argument to this script you have to provide the path to a directory (./CONF/VLAN.VIRTUAL.$HOSTNAME for example) containing valid epc and enb config files"
        exit 1
    fi
else
    echo_error "ERROR while invoking this script, as first argument to this script  you have to provide the path to a directory  (./CONF/VLAN.VIRTUAL.$HOSTNAME for example) containing valid epc and enb config files"
    exit 1
fi


test_command_install_package "tshark"   "tshark" "--force-yes"
test_command_install_package "gccxml"   "gccxml" "--force-yes"
test_command_install_package "vconfig"  "vlan"
test_command_install_package "iptables" "iptables"
test_command_install_package "iperf"    "iperf"
test_command_install_package "ip"       "iproute"
#test_command_install_script  "ovs-vsctl" "$OPENAIRCN_DIR/SCRIPTS/install_openvswitch1.9.0.bash"
test_command_install_package "tunctl"  "uml-utilities"
#test_command_install_lib     "/usr/lib/libconfig.so"  "libconfig-dev"


test_command_install_script   "asn1c" "$OPENAIRCN_DIR/SCRIPTS/install_asn1c_0.9.24.modified.bash"

# One mor check about version of asn1c
ASN1C_COMPILER_REQUIRED_VERSION_MESSAGE="ASN.1 Compiler, v0.9.24"
ASN1C_COMPILER_VERSION_MESSAGE=`asn1c -h 2>&1 | grep -i ASN\.1\ Compiler`
##ASN1C_COMPILER_VERSION_MESSAGE=`trim $ASN1C_COMPILER_VERSION_MESSAGE`
if [ "$ASN1C_COMPILER_VERSION_MESSAGE" != "$ASN1C_COMPILER_REQUIRED_VERSION_MESSAGE" ]
then
    diff <(echo -n "$ASN1C_COMPILER_VERSION_MESSAGE") <(echo -n "$ASN1C_COMPILER_REQUIRED_VERSION_MESSAGE")
    echo_error "Version of asn1c is not the required one, do you want to install the required one (overwrite installation) ? (Y/n)"
    echo_error "$ASN1C_COMPILER_VERSION_MESSAGE"
    while read -r -n 1 -s answer; do
        if [[ $answer = [YyNn] ]]; then
            [[ $answer = [Yy] ]] && $OPENAIRCN_DIR/SCRIPTS/install_asn1c_0.9.24.modified.bash
            [[ $answer = [Nn] ]] && echo_error "Version of asn1c is not the required one, exiting." && exit 1
            break
        fi
    done
fi



cd $THIS_SCRIPT_PATH
#######################################################
# FIND CONFIG FILE
#######################################################
SEARCHED_CONFIG_FILE_ENB="enb*.conf"
CONFIG_FILE_ENB=`find $CONFIG_FILE_DIR -iname $SEARCHED_CONFIG_FILE_ENB`
if [ -f $CONFIG_FILE_ENB ]; then
    echo_warning "eNB config file found is now $CONFIG_FILE_ENB"
else
    echo_error "eNB config file not found, exiting"
    exit 1
fi

#######################################################
# SOURCE CONFIG FILE
#######################################################
rm -f /tmp/source.txt
VARIABLES="
           ENB_INTERFACE_NAME_FOR_S1_MME\|\
           ENB_IPV4_ADDRESS_FOR_S1_MME\|\
           ENB_INTERFACE_NAME_FOR_S1U\|\
           ENB_IPV4_ADDRESS_FOR_S1U"

VARIABLES=$(echo $VARIABLES | sed -e 's/\\r//g')
VARIABLES=$(echo $VARIABLES | tr -d ' ')
cat $CONFIG_FILE_ENB | grep -w "$VARIABLES"| tr -d " " | tr -d ";" > /tmp/source.txt
source /tmp/source.txt

declare ENB_IPV4_NETMASK_FOR_S1_MME=$(       echo $ENB_IPV4_ADDRESS_FOR_S1_MME        | cut -f2 -d '/')
declare ENB_IPV4_NETMASK_FOR_S1U=$(          echo $ENB_IPV4_ADDRESS_FOR_S1U        | cut -f2 -d '/')

ENB_IPV4_ADDRESS_FOR_S1_MME=$(               echo $ENB_IPV4_ADDRESS_FOR_S1_MME        | cut -f1 -d '/')
ENB_IPV4_ADDRESS_FOR_S1U=$(                  echo $ENB_IPV4_ADDRESS_FOR_S1U           | cut -f1 -d '/')

is_openvswitch_interface $ENB_INTERFACE_NAME_FOR_S1_MME  \
                               $ENB_INTERFACE_NAME_FOR_S1U

if [ $? -eq 1 ]; then
   echo_success "Found open-vswitch network configuration"
else
    is_vlan_interface $ENB_INTERFACE_NAME_FOR_S1_MME \
                      $ENB_INTERFACE_NAME_FOR_S1U
    if [ $? -eq 1 ]; then
        echo_success "Found VLAN network configuration"
        clean_enb_vlan_network
        build_enb_vlan_network
        test_enb_vlan_network
    else
        echo_error "Cannot find open-vswitch network configuration or VLAN network configuration"
        exit 1
    fi 
fi


#######################################################
# USIM, NVRAM files
#######################################################
export NVRAM_DIR=$THIS_SCRIPT_PATH

if [ ! -f $OPENAIRCN_DIR/NAS/EURECOM-NAS/bin/ue_data ]; then
    make --directory=$OPENAIRCN_DIR/NAS/EURECOM-NAS veryveryclean
    make --directory=$OPENAIRCN_DIR/NAS/EURECOM-NAS PROCESS=UE
    rm .ue.nvram
fi
if [ ! -f $OPENAIRCN_DIR/NAS/EURECOM-NAS/bin/usim_data ]; then
    make --directory=$OPENAIRCN_DIR/NAS/EURECOM-NAS veryveryclean
    make --directory=$OPENAIRCN_DIR/NAS/EURECOM-NAS PROCESS=UE
    rm .usim.nvram
fi
if [ ! -f .ue.nvram ]; then
    # generate .ue_emm.nvram .ue.nvram
    $OPENAIRCN_DIR/NAS/EURECOM-NAS/bin/ue_data --gen
fi

if [ ! -f .usim.nvram ]; then
    # generate .usim.nvram
    $OPENAIRCN_DIR/NAS/EURECOM-NAS/bin/usim_data --gen
fi
$OPENAIRCN_DIR/NAS/EURECOM-NAS/bin/ue_data --print
$OPENAIRCN_DIR/NAS/EURECOM-NAS/bin/usim_data --print

##################################################
# LAUNCH eNB + UE executable
##################################################
echo "Bringup UE interface"
pkill oaisim
bash_exec "rmmod $IP_DRIVER_NAME" > /dev/null 2>&1

cecho "make $MAKE_IP_DRIVER_TARGET $MAKE_LTE_ACCESS_STRATUM_TARGET ....." $green
#bash_exec "make --directory=$OPENAIR2_DIR $MAKE_IP_DRIVER_TARGET "
make --directory=$OPENAIR2_DIR $MAKE_IP_DRIVER_TARGET || exit 1

#bash_exec "make --directory=$OPENAIR_TARGETS/SIMU/USER $MAKE_LTE_ACCESS_STRATUM_TARGET "
make --directory=$OPENAIR_TARGETS/SIMU/USER $MAKE_LTE_ACCESS_STRATUM_TARGET -j`grep -c ^processor /proc/cpuinfo ` || exit 1


bash_exec "insmod  $OPENAIR2_DIR/NETWORK_DRIVER/UE_IP/$IP_DRIVER_NAME.ko"

bash_exec "ip route flush cache"

#bash_exec "ip link set $LTEIF up"
sleep 1
#bash_exec "ip addr add dev $LTEIF $UE_IPv4_CIDR"
#bash_exec "ip addr add dev $LTEIF $UE_IPv6_CIDR"

sleep 1

bash_exec "sysctl -w net.ipv4.conf.all.log_martians=1"
assert "  `sysctl -n net.ipv4.conf.all.log_martians` -eq 1" $LINENO

echo "   Disabling reverse path filtering"
bash_exec "sysctl -w net.ipv4.conf.all.rp_filter=0"
assert "  `sysctl -n net.ipv4.conf.all.rp_filter` -eq 0" $LINENO


bash_exec "ip route flush cache"

# Check table 200 lte in /etc/iproute2/rt_tables
fgrep lte /etc/iproute2/rt_tables  > /dev/null 
if [ $? -ne 0 ]; then
    echo "200 lte " >> /etc/iproute2/rt_tables
fi
ip rule add fwmark 5 table lte
ip route add default dev $LTEIF table lte

ITTI_LOG_FILE=./itti_enb.$HOSTNAME.log
rotate_log_file $ITTI_LOG_FILE
STDOUT_LOG_FILE=./stdout_enb_ue.log

rotate_log_file $STDOUT_LOG_FILE
rotate_log_file $STDOUT_LOG_FILE.filtered
rotate_log_file tshark.pcap

cd $THIS_SCRIPT_PATH

nohup tshark -i $ENB_INTERFACE_NAME_FOR_S1_MME -i $ENB_INTERFACE_NAME_FOR_S1U -w tshark.pcap &

nohup xterm -e $OPENAIRCN_DIR/NAS/EURECOM-NAS/bin/UserProcess &

gdb --args $OPENAIR_TARGETS/SIMU/USER/oaisim -a -u1 -l9 -K $ITTI_LOG_FILE --enb-conf $CONFIG_FILE_ENB 2>&1 | tee $STDOUT_LOG_FILE 

pkill tshark

cat $STDOUT_LOG_FILE |  grep -v '[PHY]' | grep -v '[MAC]' | grep -v '[EMU]' | \
                        grep -v '[OCM]' | grep -v '[OMG]' | \
                        grep -v 'RLC not configured' | grep -v 'check if serving becomes' | \
                        grep -v 'mac_rrc_data_req'   | grep -v 'BCCH request =>' > $STDOUT_LOG_FILE.filtered
