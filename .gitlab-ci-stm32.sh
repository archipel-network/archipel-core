#!/bin/bash

# CI helper script for STM32 builds
# This script is needed for CI runners which feature a physical STM32
# evaluation board. It ensures there is no conflicting access to the
# board and the toolchain is configured properly.

# We want to exit the script right after an error occured
set -o errexit

# CI Cache directory
CDIR=cicache
# CI temp directory
TDIR=citemp
# Lockfile for board usage
RES_LOCKFILE=$HOME/.stm32.lock

FREERTOS_VER='9.0.0'

function prepare_freertos {
    if [[ ! -d $CDIR/freertos ]]; then
        echo 'Downloading and extracting FreeRTOS...'
        mirr='https://downloads.sourceforge.net/project/freertos/FreeRTOS'
        wget -O $CDIR/freertos.zip $mirr/V$FREERTOS_VER/FreeRTOSv$FREERTOS_VER.zip
        unzip -d $CDIR/freertos $CDIR/freertos.zip
        rm -fv $CDIR/freertos.zip
        if [[ ! -d $CDIR/freertos/FreeRTOSv$FREERTOS_VER ]]; then
            echo "Directory $CDIR/freertos/FreeRTOSv$FREERTOS_VER not found!"
            exit 1
        fi
    fi
}

function prepare_venv {
    if [[ ! -d $TDIR/venv ]]; then
        echo 'Preparing virtual environment...'
        make VENV=$TDIR/venv virtualenv
    fi
}

function generate_configmk {
    echo 'Generating config.mk file...'
    cat << EOM > config.mk
TOOLCHAIN_STM32 ?= /usr/bin/arm-none-eabi-
FREERTOS_PREFIX ?= $(pwd)/$CDIR/freertos/FreeRTOSv$FREERTOS_VER
EOM
}

function start_cmd_async {
    echo "Running $1 asynchronously..."
    $1 &
    lastpid=$!
    # Ensure that program is running and did not fail
    sleep 0.5
    if ! ps -p $lastpid > /dev/null; then
        echo "Could not start $1 (PID: $lastpid)"
        exit 1
    fi
}

function start_cmd_timeout {
    echo "Running $1 ..."
    local TIMEOUT_SEC=$(($2*60))
    timeout $TIMEOUT_SEC $1
}

function force_stop {
    if pidof $1 > /dev/null; then
        echo "Stopping $1 processes..."
        killall -TERM $1 || true
        sleep 0.5
        killall -KILL $1 2> /dev/null || true
    fi
}

function force_unlock_res {
    kill -TERM $(ps ax | grep stm32_mtcp_proxy.py | grep python | tr -s ' ' | sed 's/^ *//g' | cut -d ' ' -f 1) 2> /dev/null || true
    killall -TERM openocd 2> /dev/null || true
    sleep 0.5
    kill -KILL $(ps ax | grep stm32_mtcp_proxy.py | grep python | tr -s ' ' | sed 's/^ *//g' | cut -d ' ' -f 1) 2> /dev/null || true
    killall -KILL openocd 2> /dev/null || true
    rm -f $TDIR/res.lock
    rm -f "$RES_LOCKFILE"
}

function lock_res {
    local attempt=1
    local max=48 # 4 minutes if waiting 5 sec each
    local final_attempt=0
    if [[ -r $TDIR/res.lock ]]; then
        return 0 # resources are already locked for us
    fi
    echo 'Trying to lock resources...'
    while ! (set -o noclobber; echo "." > "$RES_LOCKFILE") 2> /dev/null; do
    attempt=$[attempt + 1]
    if [[ $attempt -gt $max ]]; then
        if [[ $final_attempt -eq 1 ]]; then
            echo 'Error while locking/unlocking resources'
            exit 1
        fi
        echo 'Could not lock resources, forcing unlock'
        force_unlock_res
        final_attempt=1
        continue
    fi
    sleep 5
    done
    touch $TDIR/res.lock
}

function unlock_res {
    # Check if WE locked the board and if necessary unlock it
    # ...and kill all accessing processes
    if [[ -r $TDIR/res.lock ]]; then
        force_unlock_res
    fi
}

function flash_ud3tn {
    local attempt=1
    local max=3
    while ! make "${1:-}"; do
    attempt=$[attempt + 1]
    if [[ $attempt -gt $max ]]; then
        echo 'Flashing uD3TN failed'
        exit 1
    fi
    echo 'Trying again after 3 seconds...'
    sleep 3
    # openocd helps us to reset the board
    timeout --signal=INT --kill-after=6 5 openocd || true
    done
}

function cleanup_connection {
    EXIT_CODE=$? # preserve exit code
    echo 'Cleaning up...'
    # Kill all subprocesses which are still there...
    for processid in $(pgrep -P $$); do
        if ps --pid $processid > /dev/null; then
            echo -n "Killing PID $processid "
            echo "($(ps --pid $processid --format command --no-headers))..."
            kill -INT $processid 2> /dev/null || true
            kill -TERM $processid 2> /dev/null || true
            sleep 0.5
            kill -KILL $processid 2> /dev/null || true
        fi
    done
    unlock_res
    exit $EXIT_CODE
}

# Ensure CI dirs are present
mkdir -pv $CDIR
mkdir -pv $TDIR
# CI cache identifiers
CI_CACHE_VER="RTOS=$FREERTOS_VER,REQ=$PYTHON_REQ_HASH"
# Check cache
if [[ ! -r $CDIR/version || "$(cat $CDIR/version)" != "$CI_CACHE_VER" ]]; then
    echo 'Caching identifier changed, forcing dependency rebuild'
    rm -rf $CDIR
    mkdir -v $CDIR
    echo "$CI_CACHE_VER" > $CDIR/version
fi
# Cleanup on exit
trap cleanup_connection INT TERM EXIT

case $1 in
    # The following marker has to be available in front of every operation
    # which should be printed by the help output
    #op#
    unittest)
    # Build tests
    prepare_freertos
    generate_configmk
    echo 'Building tests'
    make clean
    make unittest-stm32
    # Flash tests
    lock_res
    echo 'Flashing tests to the board...'
    flash_ud3tn "flash-unittest-stm32-openocd-oneshot"
    sleep 1
    echo 'Running tests...'
    timeout --kill-after=6 5 openocd -c "script openocd.cfg" -c "reset halt" -c "reset run" | tee "$TDIR/ud3tn.out"
    grep -F "uD3TN unittests succeeded" "$TDIR/ud3tn.out"
    ;;
    #op#
    integrationtest)
    prepare_freertos
    generate_configmk
    prepare_venv
    echo 'Building uD3TN and tools...'
    make clean
    make stm32 type=release
    # Flash uD3TN
    lock_res
    echo 'Flashing uD3TN to the board...'
    flash_ud3tn "flash-stm32-openocd-oneshot" "type=release"
    sleep 1
    echo 'Running test scenario...'
    source $TDIR/venv/bin/activate
    start_cmd_async "make connect"
    sleep 1
    make integration-test-stm32
    ;;
    *)
    echo 'Usage: .gitlab-ci-stm32.sh <operation> [args]'
    echo '<operation> can be one of:'
    IFS=$'\r\n' GLOBIGNORE='*' command eval  \
        'array=($(awk "/\#op\#/{getline;print}" \
        .gitlab-ci-stm32.sh | sed "s/\(.*\))/\1 /"))'
    for operation in "${array[@]}"
    do
          echo "$operation"
    done
    ;;
esac
