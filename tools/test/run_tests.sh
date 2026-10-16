#!/bin/bash
# All tests: tools/test/run_tests.sh [--quick]
#   1. every core notification has a handler           (seconds)
#   2. screen tests in the emulator                    (about 40 s per case, --quick: 3 cases)
#   3. core parity: Amiga build against native build   (a few minutes; skipped with --quick)
# Needs Docker running (the emulator and the cross compiler). Exit status 0 = all passed.
cd "$(dirname "$0")/../.."
fail=0
echo "== notifications";  python3 tools/test/check_handlers.py || fail=1
echo "== screens";        python3 tools/test/screens.py "$@" || fail=1
if [ "$1" != "--quick" ]; then echo "== core parity"; python3 tools/test/core_parity.py || fail=1; fi
[ $fail = 0 ] && echo "ALL TESTS PASSED" || echo "TESTS FAILED"
exit $fail
