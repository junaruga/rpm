#!/bin/bash

# NAME
#    run-in-docker.sh - Run command on container
#
# SYNOPSIS
#     run-in-docker.sh [COMMAND] [ARG...]
#
# ENVIRONMENT VARIABLE
#     CONTAINER_IMAGE: Docker image name.
#     BUILD_DIR: Build directory.

set -ex

DEFAULT_BUILD_DIR="$(pwd)"
BUILD_DIR=${BUILD_DIR:-${DEFAULT_BUILD_DIR}}

echo 'Running in a container...'
test "${CONTAINER_IMAGE}" != ''
docker pull "${CONTAINER_IMAGE}"
git archive HEAD | \
    docker run \
    --cap-add=SYS_ADMIN \
    --rm \
    --interactive \
    --volume "${BUILD_DIR}" \
    --workdir "${BUILD_DIR}" \
    "${CONTAINER_IMAGE}" \
    /bin/bash -cex "
        dnf -y install tar
        tar -x
        ${*}
    "
