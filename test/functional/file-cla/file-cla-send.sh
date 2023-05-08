#!/bin/bash

UD3TN_DIR="$(pwd)"

set -e

echo " > Adding file contact"
mkdir -p /tmp/filecla-test/
python3 $UD3TN_DIR/tools/aap/aap_config.py -s 0 120 10000000000 -r dtn://destination.filecla.dtn/ dtn://intermediate.filecla.dtn/ file:/tmp/filecla-test/ > /dev/null

echo " > Sending bundle to destination"
python3 $UD3TN_DIR/tools/aap/aap_send.py dtn://destination.filecla.dtn/inbox "Hello" > /dev/null
