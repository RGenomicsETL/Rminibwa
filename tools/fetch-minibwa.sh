#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
lock_file="${MINIBWA_LOCK:-$root/tools/minibwa-upstream.dcf}"

read_dcf_field() {
  awk -F': ' -v key="$1" '$1 == key { sub("^[^:]+: ", ""); print; exit }' "$lock_file"
}

default_repo="$(read_dcf_field GitRepository)"
default_commit="$(read_dcf_field Commit)"

repo="${MINIBWA_REPO:-$default_repo}"
ref="${MINIBWA_REF:-$default_commit}"
dest="${MINIBWA_DEST:-$root/.sync/minibwa}"

if [ -z "$repo" ] || [ -z "$ref" ]; then
  echo "Could not read minibwa repository/commit from $lock_file" >&2
  exit 1
fi

case "$dest" in
  /*) ;;
  *) dest="$root/$dest" ;;
esac

if [ -d "$dest/.git" ]; then
  # This directory is a disposable synchronization cache. A prior portable
  # developer build may have applied the Rminibwa patch queue in place, so
  # restore the cached checkout before moving it to the newly pinned ref.
  git -C "$dest" reset --hard HEAD >/dev/null
  git -C "$dest" clean -fdx >/dev/null
  git -C "$dest" fetch --depth 1 origin "$ref"
  git -C "$dest" checkout --detach FETCH_HEAD
  git -C "$dest" reset --hard FETCH_HEAD >/dev/null
  git -C "$dest" clean -fdx >/dev/null
else
  rm -rf "$dest"
  mkdir -p "$(dirname "$dest")"
  git clone --no-checkout "$repo" "$dest"
  git -C "$dest" fetch --depth 1 origin "$ref"
  git -C "$dest" checkout --detach FETCH_HEAD
fi

actual="$(git -C "$dest" rev-parse HEAD)"
if [ "$ref" = "$default_commit" ] && [ "$actual" != "$default_commit" ]; then
  echo "Unexpected minibwa commit: $actual (expected $default_commit)" >&2
  exit 1
fi

printf '%s\n' "$actual"
