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

# Build, run, and capture coverage for one configuration. Sources and tests are
# compiled together so definition flags (e.g. TC_SEGMENT_HEADER_ENABLED) affect both.
# Args: <variant-name> [extra compiler flags...]
run_variant() {
  variant="$1"
  shift
  work="${COV_DIR}/${variant}"
  mkdir -p "${work}"
  objs=""
  for f in ${SRCS} ${TESTS}; do
    obj="${work}/$(basename "${f%.c}").o"
    # shellcheck disable=SC2086
    gcc ${COVERAGE_CFLAGS} "$@" -c "${f}" -o "${obj}"
    objs="${objs} ${obj}"
  done
  # shellcheck disable=SC2086
  gcc --coverage ${objs} -o "${work}/unit_tests"
  "${work}/unit_tests" >/dev/null
  gcovr --root "${ROOT_DIR}" --filter "${ROOT_DIR}/src" \
    --gcov-object-directory "${work}" \
    --json "${COV_DIR}/${variant}.json"
}

# The unit tests always run with TC_SEGMENT_HEADER_ENABLED, so coverage is gathered
# for that single configuration.
run_variant segment -DTC_SEGMENT_HEADER_ENABLED

REPORT_ARGS=(--root "${ROOT_DIR}" --filter "${ROOT_DIR}/src"
            -a "${COV_DIR}/segment.json")

gcovr "${REPORT_ARGS[@]}" --html --html-details --output "${OUT_FILE}"

echo "Coverage (TC_SEGMENT_HEADER_ENABLED):"
gcovr "${REPORT_ARGS[@]}" --txt --txt-metric line
gcovr "${REPORT_ARGS[@]}" --txt --txt-metric branch

echo "Coverage HTML report written to: ${OUT_FILE}"
