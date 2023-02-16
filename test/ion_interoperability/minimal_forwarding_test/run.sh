#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

set -euo pipefail

# uD3TN + ION Interoperability test
# It is recommended to run this script in a Docker image based on the
# `ion-interop` Dockerfile provided with uD3TN as follows:
# $ docker run -it -v "$(pwd):/ud3tn" ud3tn-ion-interop:<tag> \
#   bash -c '/ud3tn/test/ion_interoperability/prepare_for_test.sh /ud3tn /ud3tn_build && cd /ud3tn_build && source /ud3tn_venv/bin/activate && test/ion_interoperability/minimal_forwarding_test/run.sh 7'

BP_VERSION=$1

# This assumes you are running the command from within the "ud3tn" directory.
UD3TN_DIR="$(pwd)"
cd "$UD3TN_DIR"

exit_handler() {
    cd "$UD3TN_DIR"

    kill -TERM $UD3TN1_PID
    kill -TERM $UD3TN2_PID
    echo "Waiting for uD3TN to exit gracefully - if it doesn't, check for sanitizer warnings."
    wait $UD3TN1_PID
    wait $UD3TN2_PID

    echo "Terminating ION (timeout 20s)..."
    (ionstop || true) &
    sleep 20

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

rm -f ion.log
rm -f /tmp/ion*log /tmp/ud3tn*.log

# Start first uD3TN instance (uD3TN1)
"$UD3TN_DIR/build/posix/ud3tn" -s $UD3TN_DIR/ud3tn1.socket -c "tcpclv3:127.0.0.1,4555" -b $BP_VERSION -e "dtn://ud3tn1.dtn/" > /tmp/ud3tn1.log 2>&1 &
UD3TN1_PID=$!

# Start second uD3TN instance (uD3TN2)
"$UD3TN_DIR/build/posix/ud3tn" -s $UD3TN_DIR/ud3tn2.socket -c "tcpclv3:127.0.0.1,4554" -b $BP_VERSION -e "dtn://ud3tn2.dtn/" > /tmp/ud3tn2.log 2>&1 &
UD3TN2_PID=$!

# Start ION instance
ulimit -n 512 # fix behavior on systems with a huge limit (e.g. if the container runtime does not change the kernel default), see: #121
ionstart -I test/ion_interoperability/minimal_forwarding_test/ionstart.rc

# UD3TN1_PID and UD3TN2_PID must be defined for this
trap exit_handler EXIT

# Configure a contact to ION in uD3TN1 which allows to reach uD3TN2
sleep 0.5
python "$UD3TN_DIR/tools/aap/aap_config.py" --socket $UD3TN_DIR/ud3tn1.socket --schedule 1 3600 10000 --reaches "dtn://ud3tn2.dtn/" ipn:1.0 tcpclv3:127.0.0.1:4556

# Send a bundle to uD3TN1, addressed to uD3TN2
PAYLOAD="THISISTHEBUNDLEPAYLOAD"
python "$UD3TN_DIR/tools/aap/aap_send.py" --socket $UD3TN_DIR/ud3tn1.socket --agentid source "dtn://ud3tn2.dtn/sink" "$PAYLOAD" &

timeout 10 stdbuf -oL python "$UD3TN_DIR/tools/aap/aap_receive.py" --socket $UD3TN_DIR/ud3tn2.socket --agentid sink --count 1 --verify-pl "$PAYLOAD" --newline -vv
