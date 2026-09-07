#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -W)"
PROJECT="${1:-E:/UNREAL/ue projects/ThirdPerson}"
if [[ "$PROJECT" == /[a-zA-Z]/* ]]; then
  DRIVE="${PROJECT:1:1}"
  PROJECT="${DRIVE^^}:/${PROJECT:3}"
fi
export SYSTEMROOT="${SYSTEMROOT:-C:/Windows}"
export WINDIR="${WINDIR:-C:/Windows}"
WINROOT="${SYSTEMROOT//\\//}"
exec "$WINROOT/System32/WindowsPowerShell/v1.0/powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$HERE/ROLLBACK.ps1" -ProjectRoot "$PROJECT"