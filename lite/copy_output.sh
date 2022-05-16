#!/bin/bash
set -e

mkdir -p /src/src

cd /tmp/xeus-python-kernel
ls
cp *python*.{js,wasm,data} /src/src

echo "============================================="
echo "Compiling wasm bindings done"
echo "============================================="
