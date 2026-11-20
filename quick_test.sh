#!/bin/sh
# Builds the 64-bit layer + tests and runs them. Usage: sh quick_test.sh
cd "$(dirname "$0")/layer" && powershell -NoProfile -Command "& .\build.bat" > ../.quick_build.log 2>&1
if grep -qE " error |error C|error LNK" ../.quick_build.log; then grep -E "error" ../.quick_build.log | head -20; exit 1; fi
./build/vro_test.exe 2>&1 | grep -E "FAIL|failures"
