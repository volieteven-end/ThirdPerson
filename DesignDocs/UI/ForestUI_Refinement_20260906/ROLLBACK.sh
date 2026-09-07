#!/usr/bin/env bash
set -euo pipefail
HERE="${BASH_SOURCE[0]%/*}"
if [[ "$HERE" == /[a-zA-Z]/* ]]; then
  DRIVE="${HERE:1:1}"
  HERE="${DRIVE^^}:/${HERE:3}"
fi
PROJECT="${1:-E:/UNREAL/ue projects/ThirdPerson}"
exec powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$HERE/ROLLBACK.ps1" -Project "$PROJECT"
