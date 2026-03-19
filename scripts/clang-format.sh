#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

DIRS="Interpreter Regex Settings Util client import src"
FILES=$(find $DIRS \
	\( -path "Interpreter/prebuilt" -o -path "Interpreter/prebuilt/*" \) -prune -o \
	-type f \( -name "*.cpp" -o -name "*.h" \) -print)

echo $FILES | xargs clang-format -i
