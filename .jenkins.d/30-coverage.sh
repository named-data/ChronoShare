#!/usr/bin/env bash
set -e

JDIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$JDIR"/util.sh

set -x

if [[ $JOB_NAME == *"code-coverage" ]]; then
    gcovr --object-directory=build \
          --output=build/coverage.xml \
          --exclude="$PWD/(tests)" \
          --root=. \
          --xml

    # Generate also a detailed HTML output
    mkdir build/coverage
    gcovr --object-directory=build \
          --output=build/coverage/index.html \
          --exclude="$PWD/(tests)" \
          --root=. \
          --html \
          --html-details
fi
