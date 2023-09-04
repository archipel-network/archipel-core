#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

set -o errexit
set -o nounset
set -o pipefail

DIRS=(
    components/aap
    components/aap2
    components/agents
    components/bundle6
    components/bundle7
    components/cla
    components/daemon
    components/spp
    components/ud3tn
)
CHECKER_ARGS=(
    -fdiagnostics-color
)

if [[ ! -r ./Makefile ]]; then
    echo 'Please run this script from the main project directory!' >&2
    exit 1
fi

CHECKER_CMD_ARGS="-p build/$2"
for arg in "${CHECKER_ARGS[@]}"; do
    CHECKER_CMD_ARGS="$CHECKER_CMD_ARGS -extra-arg=$arg"
done

CHECKER="$1"
if [[ $CHECKER == *"clang-tidy"* ]]; then
    echo 'Running clang-tidy'
    CHECKER_CMD_ARGS="$CHECKER_CMD_ARGS -header-filter=.*"
    # False positive, see: #117, https://github.com/llvm/llvm-project/issues/40656
    CHECKER_CMD_ARGS="$CHECKER_CMD_ARGS -checks=-clang-analyzer-valist.*"
    TARGETS="${2:-}"
    shift
    shift
    DIRS+=("${@}")
else
    echo 'Running clang-check'
    CHECKER_CMD_ARGS="$CHECKER_CMD_ARGS -analyze"
    TARGETS="${2:-}"
    shift
    shift
    DIRS+=("${@}")
fi

function check-dir {
    echo "Checking $1..."
    local out="$($CHECKER $CHECKER_CMD_ARGS $1/*.c 2>&1 | tee >(cat 1>&2))"
    if ! (echo "$out" | grep -i -e 'error:' -e 'warning:' > /dev/null); then
        return 0
    else
        echo "Check for $1 failed"
        return 1
    fi
}

FAIL=0
for checkdir in "${DIRS[@]}"; do
    cnt=`ls -1 $checkdir/*.c 2>/dev/null | wc -l`
    if [ $cnt != 0 ]; then
        if ! check-dir "$checkdir"; then
            FAIL=$[FAIL + 1]
        fi
    fi
done

rm -f ./*.plist

if [[ $FAIL -ne 0 ]]; then
    echo 'At least one check failed'
    exit 1
fi
