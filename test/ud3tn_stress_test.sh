#!/usr/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

# This script runs a simple bundle load test. Two uD3TN instances are set up
# in two different network namespaces that are coupled using a veth pair.
# After configuring the loop contacts, two bundles are injected using a third
# uD3TN instance.

# Requirements:
# - Linux system supporting network namespaces and veth
# - iproute2
# - All requirements of uD3TN, a ud3tn binary, and a Python venv in the
#   provided directory (run `make && make virtualenv` beforehand)

# To use TCPCL instead MTCP, replace "mtcp" with "tcpclv3" in this file.

set -e

if [[ -z "$1" || -z "$2" || -z "$3" ]]; then
    echo "Usage: $0 <ud3tn-dir> <runas-user> <bundle-size-bytes>" >&2
fi

if [[ "$EUID" -ne 0 ]]; then
  echo "Not running as root, this might fail." >&2
fi

set -u

WORK_DIR="$1"
RUNAS_USER=$2
BUNDLE_SIZE=$3
SOCK_DIR="$(mktemp -d)"

echo "Sockets and logs will be stored here: $SOCK_DIR" >&2

chown $RUNAS_USER "$SOCK_DIR"

NS1=ud3tn-stress-1
NS2=ud3tn-stress-2
VE1=ud3tn-st-veth1
VE2=ud3tn-st-veth1
IP1="10.101.101.1"
IP2="10.101.101.2"

UD3TN_1=0
UD3TN_2=0
UD3TN_3=0

cleanup() {
    [ $UD3TN_1 -ne 0 ] && pkill -P $UD3TN_1
    [ $UD3TN_2 -ne 0 ] && pkill -P $UD3TN_2
    [ $UD3TN_3 -ne 0 ] && pkill -P $UD3TN_3
    ip netns del $NS1 > /dev/null 2>&1 || true
    ip netns del $NS2 > /dev/null 2>&1 || true
}

trap cleanup EXIT
cleanup

ip netns add $NS1
ip netns add $NS2

ip link add $VE2 netns $NS2 type veth peer $VE1 netns $NS1

ip -netns $NS1 link set dev lo up
ip -netns $NS2 link set dev lo up
ip -netns $NS1 link set dev $VE1 up
ip -netns $NS2 link set dev $VE2 up

ip -netns $NS1 addr add "$IP1/24" dev $VE1
ip -netns $NS2 addr add "$IP2/24" dev $VE2

ip netns exec $NS1 sudo -u $RUNAS_USER "$WORK_DIR/build/posix/ud3tn" -c "mtcp:$IP1,4222" -e "dtn://ud3tn1.dtn/" -s "$SOCK_DIR/ud3tn1.socket" -S "$SOCK_DIR/ud3tn1.aap2.socket" > "$SOCK_DIR/ud3tn1.log" &
UD3TN_1=$!
ip netns exec $NS2 sudo -u $RUNAS_USER "$WORK_DIR/build/posix/ud3tn" -c "mtcp:$IP2,4222" -e "dtn://ud3tn2.dtn/" -s "$SOCK_DIR/ud3tn2.socket" -S "$SOCK_DIR/ud3tn2.aap2.socket" > "$SOCK_DIR/ud3tn2.log" &
UD3TN_2=$!
ip netns exec $NS1 sudo -u $RUNAS_USER "$WORK_DIR/build/posix/ud3tn" -c "mtcp:$IP1,4223" -e "dtn://ud3tn3.dtn/" -s "$SOCK_DIR/ud3tn3.socket" -S "$SOCK_DIR/ud3tn3.aap2.socket" > "$SOCK_DIR/ud3tn3.log" &
UD3TN_3=$!

# Configure
echo "Configuring contacts to send bundles in a loop." >&2
"$WORK_DIR/.venv/bin/python" "$WORK_DIR/tools/aap/aap_config.py" --socket "$SOCK_DIR/ud3tn1.socket" --schedule 1 100000000 10000000000000 --reaches ipn:1.1 dtn://ud3tn2.dtn/ "mtcp:$IP2:4222" > /dev/null
"$WORK_DIR/.venv/bin/python" "$WORK_DIR/tools/aap/aap_config.py" --socket "$SOCK_DIR/ud3tn2.socket" --schedule 1 100000000 10000000000000 --reaches ipn:1.1 dtn://ud3tn1.dtn/ "mtcp:$IP1:4222" > /dev/null
"$WORK_DIR/.venv/bin/python" "$WORK_DIR/tools/aap/aap_config.py" --socket "$SOCK_DIR/ud3tn3.socket" --schedule 1 100000000 10000000000000 --reaches ipn:1.1 dtn://ud3tn1.dtn/ "mtcp:$IP1:4222" > /dev/null

# Start it! We use two bundles so that each ud3tn instance sends one bundle in one direction at a time.
echo "Injecting two bundles with payload size $BUNDLE_SIZE bytes into the loop." >&2
cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w $BUNDLE_SIZE | head -n 1 | "$WORK_DIR/.venv/bin/python" "$WORK_DIR/tools/aap/aap_send.py" --socket "$SOCK_DIR/ud3tn3.socket" ipn:1.1
cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w $BUNDLE_SIZE | head -n 1 | "$WORK_DIR/.venv/bin/python" "$WORK_DIR/tools/aap/aap_send.py" --socket "$SOCK_DIR/ud3tn3.socket" ipn:1.1

echo "You should see some traffic now. Inspect it using: sudo ip netns exec $NS1 iftop -i $VE1" >&2

echo -n "> Press ENTER to exit and clean up..." >&2
read
