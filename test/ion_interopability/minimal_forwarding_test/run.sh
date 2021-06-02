#!/bin/bash

set -o errexit

if [ -z "$1" ] || [ $1 -eq '7' ]
then
    ION_VER="4.0.0"
    BP_VERSION=7
elif [ $1 -eq '6' ]
then
    ION_VER="3.7.1"
    BP_VERSION=6
else
    exit -1
fi

# This assumes you are running the command from within the "ud3tn" directory.
UD3TN_DIR="$(pwd)"

cd /tmp

# Download and build ION
wget -O "ion-$ION_VER.tar.gz" "https://sourceforge.net/projects/ion-dtn/files/ion-$ION_VER.tar.gz/download?use_mirror=netcologne&ts=$(date +%s)"
tar xvf "ion-$ION_VER.tar.gz"
cd "ion-open-source-$ION_VER"
./configure
make
make install
ldconfig

cd "$UD3TN_DIR"

# Compile uD3TN and create Python venv
make
make virtualenv || true
source .venv/bin/activate

exit_handler() {
    cd "$UD3TN_DIR"

    echo "Terminating ION and uD3TN..."
    ionstop || true
    kill -KILL $(ps ax | grep ud3tn | tr -s ' ' | sed 's/^ *//g' | cut -d ' ' -f 1) 2> /dev/null || true

    echo
    echo ">>> ION LOGFILE"
    cat "ion.log" || true
    echo
    echo ">>> uD3TN1 LOGFILE"
    cat "/tmp/ud3tn1.log" || true
    echo
    echo ">>> uD3TN2 LOGFILE"
    cat "/tmp/ud3tn2.log" || true
    echo
}

trap exit_handler EXIT

rm -f ion.log
rm -f /tmp/*.log

# Start first uD3TN instance (uD3TN1)
stdbuf -oL "$UD3TN_DIR/build/posix/ud3tn" -s $UD3TN_DIR/ud3tn1.socket -c "tcpclv3:127.0.0.1,4555" -b $BP_VERSION -e "dtn://ud3tn1.dtn" > /tmp/ud3tn1.log 2>&1 &

# Start second uD3TN instance (uD3TN2)
stdbuf -oL "$UD3TN_DIR/build/posix/ud3tn" -s $UD3TN_DIR/ud3tn2.socket -c "tcpclv3:127.0.0.1,4554" -b $BP_VERSION -e "dtn://ud3tn2.dtn" > /tmp/ud3tn2.log 2>&1 &

# Start ION instance
ionstart -I test/ion_interopability/minimal_forwarding_test/ionstart.rc

# Configure a contact to ION in uD3TN1 which allows to reach uD3TN2
sleep 0.5
python "$UD3TN_DIR/tools/aap/aap_config.py" --socket $UD3TN_DIR/ud3tn1.socket --schedule 1 3600 10000 --reaches "dtn://ud3tn2.dtn" ipn:1.0 tcpclv3:127.0.0.1:4556

# Send a bundle to uD3TN1, addressed to uD3TN2
PAYLOAD="THISISTHEBUNDLEPAYLOAD"
python "$UD3TN_DIR/tools/aap/aap_send.py" --socket $UD3TN_DIR/ud3tn1.socket --agentid source "dtn://ud3tn2.dtn/sink" "$PAYLOAD" &

timeout 10 stdbuf -oL python "$UD3TN_DIR/tools/aap/aap_receive.py" --socket $UD3TN_DIR/ud3tn2.socket --agentid sink --count 1 --verify-pl "$PAYLOAD"
