#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
  echo "usage: $0 vMAJOR.MINOR.PATCH [target-ref]" >&2
  exit 1
fi

tag="$1"
target="${2:-HEAD}"

case "$tag" in
  v[0-9]*.[0-9]*.[0-9]*) ;;
  *)
    echo "release tag must look like vMAJOR.MINOR.PATCH" >&2
    exit 1
    ;;
esac

git rev-parse --verify "$target" >/dev/null
git tag -a "$tag" "$target" -m "Release $tag"
echo "created annotated tag $tag on $target"
echo "push with: git push origin $tag"
