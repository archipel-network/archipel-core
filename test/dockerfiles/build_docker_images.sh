#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

# Script to build the Docker images necessary for running uD3TN's CI
# tests. Run this from the uD3TN base directory!

set -euo pipefail

if [[ ! -r test/dockerfiles/ion-interop ]]; then
    echo "Please run this script from the uD3TN base directory!"
    exit 1
fi

docker build -f test/dockerfiles/ion-interop --build-arg ION_FILE=ion-open-source-3.7.4.tar.gz -t ud3tn-ion-interop:3.7.4 .
docker build -f test/dockerfiles/ion-interop --build-arg ION_FILE=ion-open-source-4.0.1.tar.gz -t ud3tn-ion-interop:4.0.1 .
#docker build -f test/dockerfiles/ion-interop -t ud3tn-ion-interop:latest .
docker build -f test/dockerfiles/ione-interop --build-arg ION_FILE=ione-1.0.1.tar.gz -t ud3tn-ione-interop:1.0.1
docker build -f test/dockerfiles/ci-python-clang -t ci-python-clang:3.10-bullseye .

echo "You may now push the images to the registry:"
echo "\$ docker tag ud3tn-ion-interop:<tag> registry.gitlab.com/d3tn/ud3tn-docker-images/ion-interop:<tag>"
echo "\$ docker login registry.gitlab.com"
echo "\$ docker push registry.gitlab.com/d3tn/ud3tn-docker-images/ion-interop:<tag>"
