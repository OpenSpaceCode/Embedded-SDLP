#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_FILE="${1:-${ROOT_DIR}/build/coverage/index.html}"

if [[ "${OUT_FILE}" != /* ]]; then
  OUT_FILE="${ROOT_DIR}/${OUT_FILE}"
fi

COVERAGE_CFLAGS='-O0 -g --coverage -Iinclude -Wall -Wextra -Wpedantic -Wshadow -Wcast-align -Wcast-qual -Wpointer-arith -Wformat=2 -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wundef -std=c11'

SRCS="src/sdlp_tm.c src/sdlp_tc.c"
TESTS="tests/test_tm.c tests/test_tc.c tests/unit_tests.c"

cd "${ROOT_DIR}"

if ! command -v gcovr >/dev/null 2>&1; then
  echo "Error: gcovr is not installed."
  echo "Install with: pip install gcovr"
  exit 1
fi

COV_DIR="${ROOT_DIR}/build/coverage"
rm -rf "${COV_DIR}"
mkdir -p "$(dirname "${OUT_FILE}")" "${COV_DIR}"

# Build the unit tests with coverage instrumentation. Sources and tests are compiled
# together with TC_SEGMENT_HEADER_ENABLED so the segment-header code paths are built
# and exercised (matching how `make test` runs them).
objs=""
for f in ${SRCS} ${TESTS}; do
  obj="${COV_DIR}/$(basename "${f%.c}").o"
  # shellcheck disable=SC2086
  gcc ${COVERAGE_CFLAGS} -DTC_SEGMENT_HEADER_ENABLED -c "${f}" -o "${obj}"
  objs="${objs} ${obj}"
done
# shellcheck disable=SC2086
gcc --coverage ${objs} -o "${COV_DIR}/unit_tests"
"${COV_DIR}/unit_tests" >/dev/null

# Emit the HTML report and a text summary (line + branch) in a single gcovr pass, so
# the console output is not duplicated. gcovr's chatty "(INFO)" progress lines are
# filtered from stderr; warnings and errors still pass through and preserve the exit code.
echo "Coverage (TC_SEGMENT_HEADER_ENABLED):"
gcovr --root "${ROOT_DIR}" --filter "${ROOT_DIR}/src" \
      --gcov-object-directory "${COV_DIR}" \
      --html-details --output "${OUT_FILE}" \
      --txt - --txt-summary \
  2> >(grep -v '^(INFO)' >&2)

echo "Coverage HTML report written to: ${OUT_FILE}"
