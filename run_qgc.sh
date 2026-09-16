#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
for binary in "$project_dir/build/Debug/QGroundControl" "$project_dir/build/QGroundControl"; do
    if [[ -x "$binary" ]]; then
        exec "$binary" "$@"
    fi
done
echo 'Сначала соберите QGroundControl: JOBS=4 just build (см. fpv/README.md).' >&2
exit 1
