#!/bin/bash

UD3TN_DIR="$(pwd)"

set -e

echo " > Adding file contact"
mkdir -p /tmp/filecla-test/
python3 $UD3TN_DIR/tools/aap/aap_config.py -s 0 120 10000000000 -r dtn://source.filecla.dtn/ dtn://intermediate.filecla.dtn/ file:/tmp/filecla-test/ > /dev/null

echo " > Receiving bundles"
python3 $UD3TN_DIR/tools/aap/aap_receive.py -a inbox