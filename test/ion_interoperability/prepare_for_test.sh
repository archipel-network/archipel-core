#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

# Script to prepare the environment _inside_ a newly launched container for
# an ION interoperability test. Invoke as shown below.

if [[ -z "$1" || -z "$2" ]]; then
    echo "Usage: $0 <ud3tn_dir> <working_dir> [make args]"
    exit 1
fi

set -euo pipefail

UD3TN_DIR="$1"
WORK_DIR="$2"

mkdir $WORK_DIR
cp -r $UD3TN_DIR/* $WORK_DIR/

# consume $1 and $2 to forward the rest to make
shift
shift

# Build uD3TN
cd $WORK_DIR
make clean
make -j4 sanitize=yes "$@"

# Python dependencies
source /ud3tn_venv/bin/activate
pip install -e "$WORK_DIR/pyd3tn"
pip install -e "$WORK_DIR/python-ud3tn-utils"
