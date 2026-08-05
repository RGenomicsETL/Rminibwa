#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
checkout="${1:-.sync/minibwa}"

case "$checkout" in
  /*) ;;
  *) checkout="$root/$checkout" ;;
esac

if [ ! -d "$checkout" ]; then
  echo "minibwa checkout/source directory not found: $checkout" >&2
  exit 1
fi

patch_dir="$root/tools/patches/minibwa"
if [ ! -d "$patch_dir" ]; then
  echo "patch directory not found: $patch_dir" >&2
  exit 1
fi

shopt -s nullglob
patches=("$patch_dir"/*.patch)
if [ "${#patches[@]}" -eq 0 ]; then
  echo "No minibwa patches to apply."
  exit 0
fi

git_top="$(git -C "$checkout" rev-parse --show-toplevel 2>/dev/null || true)"
if [ -n "$git_top" ]; then
  checkout_rel="$(realpath --relative-to="$git_top" "$checkout")"
else
  echo "No Git worktree found for patch application: $checkout" >&2
  exit 1
fi

apply_directory=()
if [ "$checkout_rel" != "." ]; then
  apply_directory=("--directory=$checkout_rel")
fi

apply_check() {
  git -C "$git_top" apply --check "${apply_directory[@]}" "$1"
}

apply_reverse_check() {
  git -C "$git_top" apply --reverse --check "${apply_directory[@]}" "$1"
}

apply_patch() {
  git -C "$git_top" apply --whitespace=nowarn "${apply_directory[@]}" "$1"
}

for patch in "${patches[@]}"; do
  name="$(basename "$patch")"
  if apply_check "$patch" >/dev/null 2>&1; then
    echo "Applying $name"
    apply_patch "$patch"
  elif apply_reverse_check "$patch" >/dev/null 2>&1; then
    echo "Already applied $name"
  else
    echo "Patch does not apply cleanly: $name" >&2
    apply_check "$patch"
    exit 1
  fi
done
