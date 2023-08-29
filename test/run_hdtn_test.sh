#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

set -euo pipefail

# uD3TN + HDTN Interoperability test
# It is recommended to run this script in a Docker image based on the
# `hdtn_interop` Dockerfile provided with uD3TN as follows:
# $ docker run -it -v "$(pwd):/ud3tn" ud3tn-hdtn-interop:<tag> \
#   bash -c '/ud3tn/test/dockerfiles/prepare_for_test.sh /ud3tn /ud3tn_build && cd /ud3tn_build && source /ud3tn_venv/bin/activate && test/run_hdtn_test.sh'


BP_VERSION=7

# Config variables
HDTN_EID=ipn:10.0
UD3TN1_EID=ipn:1.0
UD3TN2_EID=ipn:2.0
SOURCE_AGENTID=3
SINK_AGENTID=5
SINK_EID=ipn:2.5
BPGEN_EID=ipn:1.2
HDTN_CONFIG_FILE=$HDTN_SOURCE_ROOT/config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json
HDTN_CONTACT_PLAN=$HDTN_SOURCE_ROOT/module/scheduler/src/contactPlanCutThroughMode.json
HDTN_BPGEN_CONFIG=$HDTN_SOURCE_ROOT/config_files/outducts/bpgen_one_tcpcl_port4556.json
HDTN_BPSINK_CONFIG=$HDTN_SOURCE_ROOT/config_files/inducts/bpsink_one_tcpcl_port4558.json


UD3TN_DIR="$(pwd)"
cd "$UD3TN_DIR"

exit_handler() {
    cd "$UD3TN_DIR"

    kill -TERM $UD3TN1_PID
    kill -TERM $HDTNSINK_PID
    kill -2 $HDTN_PID
    echo "Waiting for uD3TN to exit gracefully - if it doesn't, check for sanitizer warnings."
    echo "Waiting for HDTN and HDTN_bpsink to exit gracefully..."
    wait $UD3TN1_PID
    wait $HDTNSINK_PID
    wait $HDTN_PID

    echo
    echo ">>> HDTN LOGFILE"
    cat "/tmp/hdtn.log" || true
    echo
    echo ">>> uD3TN1 LOGFILE"
    cat "/tmp/ud3tn1.log" || true
    echo
}

rm -f hdtn_payload.txt received_payload.txt
rm -f /tmp/hdtn*log /tmp/ud3tn*.log /tmp/hdtn_payload.txt

# uD3TN1 -> HDTN -> uD3TN2 Test
# Start first uD3TN instance (uD3TN1)
"$UD3TN_DIR/build/posix/ud3tn" -s $UD3TN_DIR/ud3tn1.socket -c "tcpclv3:127.0.0.1,4554" -b $BP_VERSION -e "$UD3TN1_EID" > /tmp/ud3tn1.log 2>&1 &
UD3TN1_PID=$!

# Start second uD3TN instance (uD3TN2)
"$UD3TN_DIR/build/posix/ud3tn" -s $UD3TN_DIR/ud3tn2.socket -c "tcpclv3:127.0.0.1,4558" -b $BP_VERSION -e "$UD3TN2_EID" > /tmp/ud3tn2.log 2>&1 &
UD3TN2_PID=$!

# Start HDTN
"$HDTN_SOURCE_ROOT/build/module/hdtn_one_process/hdtn-one-process" --hdtn-config-file=$HDTN_CONFIG_FILE --contact-plan-file=$HDTN_CONTACT_PLAN > /tmp/hdtn.log 2>&1 &
HDTN_PID=$!

trap exit_handler EXIT

# Configure contact to HDTN in uD3TN1 which allows to reach uD3TN2
sleep 0.5
python "$UD3TN_DIR/tools/aap/aap_config.py" --socket $UD3TN_DIR/ud3tn1.socket --schedule 1 3600 10000 --reaches "$UD3TN2_EID" "$HDTN_EID" tcpclv3:127.0.0.1:4556

# Send a bundle to uD3TN1, addressed to uD3TN2
PAYLOAD="THISISTHEBUNDLEPAYLOAD"
python "$UD3TN_DIR/tools/aap/aap_send.py" --socket $UD3TN_DIR/ud3tn1.socket --agentid "$SOURCE_AGENTID" "$SINK_EID" "$PAYLOAD" &

timeout 10 stdbuf -oL python "$UD3TN_DIR/tools/aap/aap_receive.py" --socket $UD3TN_DIR/ud3tn2.socket --agentid "$SINK_AGENTID" --count 1 --verify-pl "$PAYLOAD" --newline -vv

echo $PAYLOAD > hdtn_payload.txt

# HDTN_bpgen -> HDTN -> uD3TN2 Test
# Start HDTN_bpgen and aap_receiver on uD3TN2
$HDTN_SOURCE_ROOT/build/common/bpcodec/apps/bpgen-async --my-uri-eid="$BPGEN_EID" --bundle-size=10000 --bundle-rate=10 --dest-uri-eid="$SINK_EID" --outducts-config-file=$HDTN_BPGEN_CONFIG --duration=1 &
timeout 10 stdbuf -oL python "$UD3TN_DIR/tools/aap/aap_receive.py" --socket $UD3TN_DIR/ud3tn2.socket --agentid "$SINK_AGENTID" --count 10 -vv

# Send `hdtn_payload.txt` to uD3TN aap_receiver so we could send it back
$HDTN_SOURCE_ROOT/build/common/bpcodec/apps/bpsendfile --file-or-folder-path=hdtn_payload.txt --my-uri-eid="$BPGEN_EID" --dest-uri-eid="$SINK_EID" --use-bp-version-7 --outducts-config-file=$HDTN_BPGEN_CONFIG &
HDTNSENDFILE_PID=$!
timeout 10 stdbuf -oL python "$UD3TN_DIR/tools/aap/aap_receive.py" --socket $UD3TN_DIR/ud3tn2.socket --agentid "$SINK_AGENTID" --count 1 -vv -o received_payload.txt
sleep 3

# Terminate uD3TN2 so that HDTN_ could start
echo "Terminating uD3TN_2..." && kill -2 $UD3TN2_PID
echo "Termintaing HDTN_BPsendfile..." && kill -2 $HDTNSENDFILE_PID
sleep 0.5
wait $UD3TN2_PID

# uD3TN1 -> HDTN -> HDTN_bpreceivefile Test
# Start HDTN_bpreceivefile
$HDTN_SOURCE_ROOT/build/common/bpcodec/apps/bpreceivefile --save-directory=/tmp --inducts-config-file=$HDTN_BPSINK_CONFIG --my-uri-eid="$SINK_EID" &
HDTNSINK_PID=$!

# Configure contact to HDTN in uD3TN1 which allows to reach HDTN_bpreceivefile
(sleep 2 && python "$UD3TN_DIR/tools/aap/aap_config.py" --socket $UD3TN_DIR/ud3tn1.socket --schedule 1 3600 10000 --reaches "$UD3TN2_EID" "$HDTN_EID" tcpclv3:127.0.0.1:4556)&

# Send bundle which contains `hdtn_payload.txt` to HDTN_bpreceivefile
(sleep 3 && (cat $UD3TN_DIR/received_payload.txt | python "$UD3TN_DIR/tools/aap/aap_send.py" --socket $UD3TN_DIR/ud3tn1.socket --agentid "$SOURCE_AGENTID" "$SINK_EID"))&

inotifywait /tmp -e create |
        while read -r directory action file; do
            if [[ "$file" =~ payload.txt$ ]]; then
                RECEIVED_PAYLOAD="$(cat /tmp/hdtn_payload.txt)"
                echo "Payload was received successfully."
                if [[ "$RECEIVED_PAYLOAD" != "$PAYLOAD" ]]; then
                    echo "Received payload does not match: $RECEIVED_PAYLOAD"
                    exit 1
                fi
                exit;
            fi
        done
