#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

set -euo pipefail

# uD3TN + DTN7 Interoperability Test

# It is recommended to run this script in a Docker image based on the
# `dtn7-interop` Dockerfile provided with uD3TN as follows:

# $ docker run -it -v "$(pwd):/ud3tn" registry.gitlab.com/d3tn/ud3tn-docker-images/dtn7-interop:0.19.0 bash -c '/ud3tn/test/dockerfiles/prepare_for_test.sh /ud3tn /ud3tn_build && cd /ud3tn_build && source /ud3tn_venv/bin/activate && test/dtn7_interoperability/run_dtn7_test.sh'

BP_VERSION=7

PAYLOAD="HELLO WORLD"

# Config variables
UD3TN1_EID=dtn://a.dtn/
UD3TN2_EID=dtn://b.dtn/
SINK_AGENTID=bundlesink
DTN7_EID=node1
DTN7_ROUTING=epidemic
DTN7_CLA=mtcp
DTN7_ENDPOINT=incoming
DTN7_CONFIG_FILE=test/dtn7_interoperability/dtn7-example-config.toml

# This assumes you are running the command from within the "ud3tn" directory.
UD3TN_DIR="$(pwd)"
cd "$UD3TN_DIR"

exit_handler() {
    cd "$UD3TN_DIR"

    if ps -p $UD3TN1_PID > /dev/null
    then
        kill -TERM $UD3TN1_PID
        echo "Waiting for the first uD3TN node to exit gracefully - if it doesn't, check for sanitizer warnings."
        wait $UD3TN1_PID
        echo ">>> uD3TN1 LOGFILE"
        cat "/tmp/ud3tn1.log" || true
        echo
    fi

    if ps -p $UD3TN2_PID > /dev/null
    then
        kill -TERM $UD3TN2_PID
        echo "Waiting for the second uD3TN node to exit gracefully - if it doesn't, check for sanitizer warnings."
        wait $UD3TN2_PID
        echo ">>> uD3TN2 LOGFILE"
        cat "/tmp/ud3tn2.log" || true
        echo
    fi

    echo ">>> DTN7 LOGFILE"
    cat "/tmp/dtn7.log" || true
    echo
    if ps -p $DTN7_PID > /dev/null
    then
        kill -TERM $DTN7_PID
    fi
}

rm -f /tmp/dtn7.log /tmp/ud3tn*.log /tmp/received_payload*.txt


# Set up instances

# Start first uD3TN instance (uD3TN1)
echo "Starting first uD3TN instance ..."
"$UD3TN_DIR/build/posix/ud3tn" --eid "$UD3TN1_EID" --bp-version "$BP_VERSION" --aap-port 4242 --cla "mtcp:127.0.0.1,4224" > /tmp/ud3tn1.log 2>&1 &
UD3TN1_PID=$!
#Check if it started successfully
if [ $? -ne 0 ]; then
    echo "Failed to start first uD3TN instance. Exiting."
    exit 1
fi

# Start second uD3TN instance (uD3TN2)
echo "Starting second uD3TN instance ..."
"$UD3TN_DIR/build/posix/ud3tn" --eid "$UD3TN2_EID" --bp-version "$BP_VERSION" --aap-port 4243 --cla "mtcp:127.0.0.1,4225" > /tmp/ud3tn2.log 2>&1 &
UD3TN2_PID=$!
#Check if it started successfully
if [ $? -ne 0 ]; then
    echo "Failed to start second uD3TN instance. Exiting."
    exit 1
fi

# Start DTN7 instance (along with contact to uD3TN1 via config file for later use)
echo "Starting DTN7 instance ..."
dtnd --nodeid "$DTN7_EID" --routing "$DTN7_ROUTING" --cla "$DTN7_CLA" --endpoint "$DTN7_ENDPOINT" --config "$DTN7_CONFIG_FILE" > /tmp/dtn7.log 2>&1 &
DTN7_PID=$!
#Check if it started successfully
if [ $? -ne 0 ]; then
    echo "Failed to start DTN7. Exiting."
    exit 1
fi

# Ensuring cleanup after the script exits
trap exit_handler EXIT


# Scenario 1: uD3TN1 -> DTN7

# Configure contact from uD3TN1 to DTN7
sleep 0.5
echo "Configuring contact from first uD3TN instance to DTN7 ..."
python "$UD3TN_DIR/tools/aap/aap_config.py" --tcp localhost 4242 --schedule 1 3600 100000 "dtn://$DTN7_EID/" "mtcp:127.0.0.1:16162"

# Send a bundle from uD3TN1, addressed to DTN7
echo "Sending bundle from first uD3TN instance to DTN7 ..."
python "$UD3TN_DIR/tools/aap/aap_send.py" --tcp localhost 4242 "dtn://$DTN7_EID/$DTN7_ENDPOINT" "$PAYLOAD" &

# Set up DTN7 receiver
sleep 5
timeout 10 stdbuf -oL dtnrecv --endpoint "$DTN7_ENDPOINT" > /tmp/received_payload_1.txt
# Check if received payload is correct
RECEIVED_PAYLOAD_1="$(cat /tmp/received_payload_1.txt)"
echo "Received payload 1: $RECEIVED_PAYLOAD_1"
if [[ "$RECEIVED_PAYLOAD_1" != "$PAYLOAD" ]]; then
    echo "Received payload does not match: received $RECEIVED_PAYLOAD_1 instead of $PAYLOAD"
    exit 1
fi


# Scenario 2: DTN7 -> uD3TN1

# Contact from DTN7 to uD3TN1 already configured (via config file) when starting the DTN7 instance

# Send a bundle from DTN7, addressed to uD3TN1
echo "Sending bundle from DTN7 to the first uD3TN instance ..."
(sleep 1; echo -n "$PAYLOAD" | dtnsend --receiver "$UD3TN1_EID$SINK_AGENTID") &

# Set up uD3TN1 receiver (automatically verifies if the payload is correct)
timeout 10 stdbuf -oL python "$UD3TN_DIR/tools/aap/aap_receive.py" --tcp localhost 4242 --agentid "$SINK_AGENTID" --count 1 --verify-pl "$PAYLOAD" --newline -vv > /tmp/received_payload_2.txt
RECEIVED_PAYLOAD_2="$(cat /tmp/received_payload_2.txt)"
echo "Received payload 2: $RECEIVED_PAYLOAD_2"


# Scenario 3: uD3TN2 -> DTN7 -> uD3TN1

# Configure contact from uD3TN2 to DTN7
sleep 0.5
echo "Configuring contact from second uD3TN instance to DTN7 ..."
python "$UD3TN_DIR/tools/aap/aap_config.py" --tcp localhost 4243 --schedule 1 3600 100000 --reaches "$UD3TN1_EID" "dtn://$DTN7_EID/" "mtcp:127.0.0.1:16162"

# Contact from DTN7 to uD3TN1 already set up in Scenario 2

# Send bundle from uD3TN2 to uD3TN1, via DTN7
echo "Sending bundle from second uD3TN instance, via DTN7, to the first uD3TN instance ..."
(sleep 1; python "$UD3TN_DIR/tools/aap/aap_send.py" --tcp localhost 4243 "$UD3TN1_EID$SINK_AGENTID" "$PAYLOAD") &

# Set up uD3TN1 receiver (automatically verifies if the payload is correct)
timeout 10 stdbuf -oL python "$UD3TN_DIR/tools/aap/aap_receive.py" --tcp localhost 4242 --agentid "$SINK_AGENTID" --count 1 --verify-pl "$PAYLOAD" --newline -vv > /tmp/received_payload_3.txt
RECEIVED_PAYLOAD_3="$(cat /tmp/received_payload_3.txt)"
echo "Received payload 3: $RECEIVED_PAYLOAD_3"
