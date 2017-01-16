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

    # # Generate also a detailed HTML output, but using lcov (slower, but better results)
    lcov -q -c -d . --no-external -o build/coverage-with-tests.info
    lcov -q -r build/coverage-with-tests.info "$PWD/tests/*" -o build/coverage.info
    genhtml build/coverage.info --output-directory build/coverage --legend
fi
