#!/bin/bash
echo "=== Compilando Navia Browser ==="
cd "$(dirname "$0")/build"
cmake .. -DCMAKE_PREFIX_PATH=/opt/qt/6.11.2/gcc_64 2>&1 | tail -3
make -j$(nproc) 2>&1 | tail -5
rc=${PIPESTATUS[0]}
if [ $rc -eq 0 ]; then
    echo "=== Listo: build/NaviaBrowser ==="
    exit 0
else
    echo "=== Error en la compilación (make=$rc) ==="
    exit 1
fi