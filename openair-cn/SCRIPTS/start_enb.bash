#!/bin/bash
# start MME+S/P-GW with openvswitch setting


#        +-----------+          +------+              +-----------+
#        |  eNB      +------+   |  ovs | VLAN 1+------+    MME    |
#        |           |cpenb0+------------------+cpmme0|           |
#        |           +------+   |bridge|       +------+           |
#        |           |upenb0+-------+  |              |           |
#        +-----------+------+   |   |  |              +-----------+
#                               +---|--+                    |
#                                   |                 +-----------+
#                                   |                 |  S+P-GW   |
#                                   |  VLAN2   +------+           +-------+
#                                   +----------+upsgw0|           |eth0   +---Internet access
#                                              +------+           +-------+
#                                                     |           |
#                                                     +-----------+
#


BRIDGE="vswitch"

###########################################################
IPTABLES=/sbin/iptables
THIS_SCRIPT_PATH=$(dirname $(readlink -f $0))
declare -x OPENAIR_DIR=""
declare -x OPENAIR1_DIR=""
declare -x OPENAIR2_DIR=""
declare -x OPENAIR3_DIR=""
declare -x OPENAIR_TARGETS=""
###########################################################
cidr2mask() {
  local i mask=""
  local full_octets=$(($1/8))
  local partial_octet=$(($1%8))

  for ((i=0;i<4;i+=1)); do
    if [ $i -lt $full_octets ]; then
      mask+=255
    elif [ $i -eq $full_octets ]; then
      mask+=$((256 - 2**(8-$partial_octet)))
    else
      mask+=0
    fi
    test $i -lt 3 && mask+=.
  done

  echo $mask
}


black='\E[30m'
red='\E[31m'
green='\E[32m'
yellow='\E[33m'
blue='\E[34m'
magenta='\E[35m'
cyan='\E[36m'
white='\E[37m'

ROOT_UID=0
E_NOTROOT=67

trim ()
{
    echo "$1" | sed -n '1h;1!H;${;g;s/^[ \t]*//g;s/[ \t]*$//g;p;}'
}


cecho()   # Color-echo
# arg1 = message
# arg2 = color
{
    local default_msg="No Message."
    message=${1:-$default_msg}
    color=${2:-$black}
    echo -e "$color"
    echo -n "$message"
    tput sgr0
    echo
    return
}

echo_error() {
    local my_string=""
    until [ -z "$1" ]
    do
        my_string="$my_string$1"
        shift
        done
        cecho "$my_string" $red
}

echo_warning() {
    local my_string=""
    until [ -z "$1" ]
    do
        my_string="$my_string$1"
        shift
    done
    cecho "$my_string" $yellow
}

echo_success() {
    local my_string=""
    until [ -z "$1" ]
    do
        my_string="$my_string$1"
        shift
    done
    cecho "$my_string" $green
}

bash_exec() {
    output=$($1 2>&1)
    result=$?
    if [ $result -eq 0 ]
    then
        echo_success "$1"
    else
        echo_error "$1: $output"
    fi
}

set_openair() {
    path=`pwd`
    declare -i length_path
    declare -i index
    length_path=${#path}

    index=`echo $path | grep -b -o 'targets' | cut -d: -f1`
    #echo ${path%$token*}
    if [[ $index -lt $length_path  && index -gt 0 ]]
       then
           declare -x OPENAIR_DIR
           index=`expr $index - 1`
           openair_path=`echo $path | cut -c1-$index`
           #openair_path=`echo ${path:0:$index}`
           export OPENAIR_DIR=$openair_path
           export OPENAIR1_DIR=$openair_path/openair1
           export OPENAIR2_DIR=$openair_path/openair2
           export OPENAIR3_DIR=$openair_path/openair3
           export OPENAIR_TARGETS=$openair_path/targets
           return 0
    fi
    index=`echo $path | grep -b -o 'openair3' | cut -d: -f1`
    if [[ $index -lt $length_path  && index -gt 0 ]]
       then
           declare -x OPENAIR_DIR
           index=`expr $index - 1`
           openair_path=`echo $path | cut -c1-$index`
           #openair_path=`echo ${path:0:$index}`
           export OPENAIR_DIR=$openair_path
           export OPENAIR1_DIR=$openair_path/openair1
           export OPENAIR2_DIR=$openair_path/openair2
           export OPENAIR3_DIR=$openair_path/openair3
           export OPENAIR_TARGETS=$openair_path/targets
           return 0
    fi
    return -1
}

wait_process_started () {
    if  [ -z "$1" ]
    then
        echo_error "WAITING FOR PROCESS START: NO PROCESS"
        return 1
    fi
    ps -C $1 > /dev/null 2>&1
    while [ $? -ne 0 ]; do
        echo_warning "WAITING FOR $1 START"
        sleep 2
        ps -C $1 > /dev/null 2>&1
    done
    echo_success "PROCESS $1 STARTED"
    return 0
}

is_process_started () {
    if  [ -z "$1" ]
    then
        echo_error "WAITING FOR PROCESS START: NO PROCESS"
        return 1
    fi
    ps -C $1 > /dev/null 2>&1
    if [ $? -ne 0 ]
    then
        echo_success "PROCESS $1 NOT STARTED"
        return 1
    fi
    echo_success "PROCESS $1 STARTED"
    return 0
}

assert() {
    # If condition false
    # exit from script with error message
    E_PARAM_ERR=98
    E_PARAM_FAILED=99

    if [ -z "$2" ] # Not enought parameters passed.
    then
        return $E_PARAM_ERR
    fi

    lineno=$2
    if [ ! $1 ]
    then
        echo "Assertion failed:  \"$1\""
        echo "File \"$0\", line $lineno"
        exit $E_ASSERT_FAILED
    fi
}

start_openswitch_daemon() {
  rmmod -s bridge
  is_process_started "ovsdb-server"
  if [ $? -ne 0 ]
  then
      ovsdb-server --remote=punix:/usr/local/var/run/openvswitch/db.sock --remote=db:Open_vSwitch,manager_options --pidfile --detach
      wait_process_started "ovsdb-server"
  fi
  # To be done after installation
  # ovs-vsctl    --no-wait init
  is_process_started "ovs-vswitchd"
  if [ $? -ne 0 ]
  then
      ovs-vswitchd --pidfile --detach
      wait_process_started "ovs-vswitchd"
  fi
}

set_openair
cecho "OPENAIR_DIR     = $OPENAIR_DIR" $green
cecho "OPENAIR1_DIR    = $OPENAIR1_DIR" $green
cecho "OPENAIR2_DIR    = $OPENAIR2_DIR" $green
cecho "OPENAIR3_DIR    = $OPENAIR3_DIR" $green
cecho "OPENAIR_TARGETS = $OPENAIR_TARGETS" $green




rm -f /tmp/source.txt
cat $OPENAIR3_DIR/OPENAIRMME/UTILS/CONF/mme_default.conf | tr -d " " > /tmp/source.txt
source /tmp/source.txt

rm -f /tmp/source.txt
cat $OPENAIR3_DIR/OPENAIRMME/UTILS/CONF/enb_default.conf | tr -d " " > /tmp/source.txt
source /tmp/source.txt


##################################################
# LAUNCH eNB + UE executable
##################################################
# ONLY oai interface for UE
declare MAKE_IP_DRIVER_TARGET="ue_ip.ko"
declare MAKE_LTE_ACCESS_STRATUM_TARGET="oaisim USE_MME=R10"
declare IP_DRIVER_NAME="ue_ip"
declare LTEIF="oip1"
UE_IPv4="10.0.0.8"
UE_IPv6="2001:1::8"
UE_IPv6_CIDR=$UE_IPv6"/64"
UE_IPv4_CIDR=$UE_IPv4"/24"
#------------------------------------------------
declare -a NAS_IMEI=( 3 9 1 8 3 6 6 2 0 0 0 0 0 0 )



echo "Bringup UE interface"
bash_exec "rmmod $IP_DRIVER_NAME"
cecho "make $MAKE_IP_DRIVER_TARGET $MAKE_LTE_ACCESS_STRATUM_TARGET ....." $green
# bash_exec "make --directory=$OPENAIR_TARGETS/SIMU/EXAMPLES/VIRT_EMUL_1eNB $MAKE_LTE_ACCESS_STRATUM_TARGET "
bash_exec "make --directory=$OPENAIR2_DIR $MAKE_IP_DRIVER_TARGET "
#bash_exec "make --directory=$OPENAIR2_DIR/NAS/DRIVER/LITE/RB_TOOL "

#bash_exec "insmod  $OPENAIR2_DIR/NAS/DRIVER/LITE/$IP_DRIVER_NAME.ko oai_nw_drv_IMEI=${NAS_IMEI[0]},${NAS_IMEI[1]},${NAS_IMEI[2]},${NAS_IMEI[3]},${NAS_IMEI[4]},${NAS_IMEI[5]},${NAS_IMEI[6]},${NAS_IMEI[7]},${NAS_IMEI[8]},${NAS_IMEI[9]},${NAS_IMEI[10]},${NAS_IMEI[11]},${NAS_IMEI[12]},${NAS_IMEI[13]}"
bash_exec "insmod  $OPENAIR2_DIR/NAS/DRIVER/UE_LTE/$IP_DRIVER_NAME.ko"

bash_exec "ip route flush cache"

#bash_exec "ip link set $LTEIF broadcast ff:ff:ff:ff:ff:ff"
bash_exec "ip link set $LTEIF up"
sleep 1
bash_exec "ip addr add dev $LTEIF $UE_IPv4_CIDR"
bash_exec "ip addr add dev $LTEIF $UE_IPv6_CIDR"

# -a     -> Add RB
# -d    -> Delete RB
# -cxx  -> lcr
# -ixx  -> instance
# -zxx  -> dscp
# -fxxx -> classref (uid of a classifier entry) if fn is used , fn is used for send classifier and n+1 for receive classifier
# -sxxx -> source ipv4 address
# -txxx -> destination ipv4 address
# -x    -> source ipv6 address
# -y    -> destination ipv6 address
# -r    -> radio bearer id
#bash_exec "$OPENAIR2_DIR/NAS/DRIVER/LITE/RB_TOOL/rb_tool -a -c0 -f0 -i1 -z0  -x 0::0/128     -y 0::0/128     -r 5"
#bash_exec "$OPENAIR2_DIR/NAS/DRIVER/LITE/RB_TOOL/rb_tool -a -c0 -f2 -i1 -z64 -s 0.0.0.0/32   -t 0.0.0.0/32   -r 5"
sleep 1

bash_exec "sysctl -w net.ipv4.conf.all.log_martians=1"
assert "  `sysctl -n net.ipv4.conf.all.log_martians` -eq 1" $LINENO

echo "   Disabling reverse path filtering"
bash_exec "sysctl -w net.ipv4.conf.all.rp_filter=0"
assert "  `sysctl -n net.ipv4.conf.all.rp_filter` -eq 0" $LINENO


bash_exec "ip route flush cache"

# please add table 200 lte in/etc/iproute2/rt_tables
ip rule add fwmark 5  table lte
ip route add default dev $LTEIF table lte

#gdb --args $OPENAIR_TARGETS/SIMU/USER/oaisim -a -u1 -l7 --mme_ip_address $MME_IP_ADDRESS_FOR_S1_MME --s1c_ip_address $ENB_IP_ADDRESS_FOR_S1_MME --s1u_ip_address $ENB_IP_ADDRESS_FOR_S1U
echo $OPENAIR_TARGETS/SIMU/USER/oaisim -a -u1 -l7 --mme_ip_address $MME_IP_ADDRESS_FOR_S1_MME --s1c_ip_address $ENB_IP_ADDRESS_FOR_S1_MME --s1u_ip_address $ENB_IP_ADDRESS_FOR_S1U


