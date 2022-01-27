#!/bin/bash

set -o errexit

# This assumes you are running the command from within the "ud3tn" directory.
UD3TN_DIR="$(pwd)"

# Compile uD3TN and create Python venv
make CFLAGS=-DBIBE_CL_DRAFT_1_COMPATIBILITY
make virtualenv || true
source .venv/bin/activate
cd /tmp

# Download and build ION
wget -O "ion-open-source-4.1.0.tar.gz" "https://sourceforge.net/projects/ion-dtn/files/ion-open-source-4.1.0.tar.gz/download?use_mirror=netcologne&ts=$(date +%s)"
tar xvf "ion-open-source-4.1.0.tar.gz"
cd "ion-open-source-4.1.0"
./configure
make
make install
ldconfig

cd "$UD3TN_DIR"


exit_handler() {
    cd "$UD3TN_DIR"

    echo "Terminating ION and uD3TN..."
    ionstop || true
    kill -KILL $(ps ax | grep ud3tn | tr -s ' ' | sed 's/^ *//g' | cut -d ' ' -f 1) 2> /dev/null || true

    echo
    echo ">>> ION LOGFILE"
    cat "ion.log" || true
    echo
    echo ">>> LOWER1 LOGFILE"
    cat "/tmp/lower1.log" || true
    echo
    echo ">>> LOWER2 LOGFILE"
    cat "/tmp/lower2.log" || true
    echo
    echo ">>> UPPER1 LOGFILE"
    cat "/tmp/upper1.log" || true
    echo
    echo ">>> UPPER2 LOGFILE"
    cat "/tmp/upper2.log" || true
    echo
}

trap exit_handler EXIT

rm -f ion.log
rm -f /tmp/*.log

# Start first uD3TN instance (lower1)
stdbuf -oL "$UD3TN_DIR/build/posix/ud3tn" -a localhost -p 4242 -e dtn://lower1.dtn/ -c "tcpclv3:127.0.0.1,4555;mtcp:127.0.0.1,4224" > /tmp/lower1.log 2>&1 &

# Start second uD3TN instance (upper1)
stdbuf -oL "$UD3TN_DIR/build/posix/ud3tn" -a localhost -p 4243 -e dtn://upper1.dtn/ -c "bibe:," > /tmp/upper1.log 2>&1 &

# Start third uD3TN instance (lower2)
stdbuf -oL "$UD3TN_DIR/build/posix/ud3tn" -a localhost -p 4244 -e dtn://lower2.dtn/ -c "tcpclv3:127.0.0.1,4554" > /tmp/lower2.log 2>&1 &

# Start fourth uD3TN instance (upper2)
stdbuf -oL "$UD3TN_DIR/build/posix/ud3tn" -a localhost -p 4245 -e dtn://upper2.dtn/ -c "bibe:," > /tmp/upper2.log 2>&1 &

# Start ION instance
ionstart -I test/ion_interopability/bibe_forwarding_test/ionstart.rc

# Configure contacts
sleep 3.5
HOST="$(hostname)"
python "$UD3TN_DIR/tools/aap/aap_config.py" --tcp localhost 4243 --dest_eid dtn://upper1.dtn/ --schedule 1 3600 100000 -r dtn://upper2.dtn/ dtn://lower1.dtn/ bibe:localhost:4242#dtn://$HOST/
sleep 1.5
python "$UD3TN_DIR/tools/aap/aap_config.py" --tcp localhost 4242 --schedule 1 3600 10000 --reaches "dtn://$HOST/" dtn://ion.dtn/ tcpclv3:127.0.0.1:4556
sleep 1.5
python "$UD3TN_DIR/tools/aap/aap_config.py" --tcp localhost 4244 --dest_eid dtn://lower2.dtn/ --schedule 1 3600 10000 dtn://ion.dtn/ tcpclv3:127.0.0.1:4556
sleep 1.5
python "$UD3TN_DIR/tools/aap/aap_config.py" --tcp localhost 4245 --dest_eid dtn://upper2.dtn/ --schedule 1 3600 100000 -r dtn://upper2.dtn/ dtn://lower2.dtn/ bibe:localhost:4244
sleep 1.5
# Send a BIBE bundle to lower1
PAYLOAD="THISISTHEBUNDLEPAYLOAD"
python "$UD3TN_DIR/tools/cla/bibe_over_mtcp_test.py" -l localhost -p 4224 --payload "$PAYLOAD" -i "dtn://upper2.dtn/bundlesink" -o "dtn://lower1.dtn/" --compatibility &

timeout 10 stdbuf -oL python "$UD3TN_DIR/tools/aap/aap_receive.py" --tcp localhost 4245 -a bundlesink --count 1 --verify-pl "$PAYLOAD"
