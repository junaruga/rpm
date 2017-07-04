#!/bin/bash

set -ex

echo 'Running tests on Container...'
test "${DOCKER_IMAGE}" != ''
docker pull "${DOCKER_IMAGE}"
BUILD_DIR="$(pwd)"
git archive HEAD | \
    docker run \
    --cap-add=SYS_ADMIN \
    --rm \
    --interactive \
    --volume "${BUILD_DIR}" \
    --workdir "${BUILD_DIR}" \
    "${DOCKER_IMAGE}" \
    /bin/sh -cx 'dnf -y install tar && tar -x && scripts/build.sh all'
