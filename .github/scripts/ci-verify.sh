#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
python "$root/tests/test_shared_ci.py"
exec python "$root/.github/scripts/ci-driver.py"
