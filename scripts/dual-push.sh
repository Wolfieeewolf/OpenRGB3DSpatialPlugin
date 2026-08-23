#!/usr/bin/env bash
# Push the current branch (or given ref) to both GitLab (origin) and GitHub.
# Does not modify git config.
#
# Usage:
#   scripts/dual-push.sh              # push current branch to origin + github
#   scripts/dual-push.sh HEAD:main    # push HEAD as main on both
#   scripts/dual-push.sh --tags       # also push tags
#   scripts/dual-push.sh v0.1.0       # push a single tag to both remotes
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PUSH_TAGS=0
REF=""

for arg in "$@"; do
  case "$arg" in
    --tags) PUSH_TAGS=1 ;;
    *)
      if [ -n "$REF" ]; then
        echo "Unexpected argument: $arg" >&2
        exit 1
      fi
      REF="$arg"
      ;;
  esac
done

if [ -z "$REF" ]; then
  REF="$(git rev-parse --abbrev-ref HEAD)"
  if [ "$REF" = "HEAD" ]; then
    echo "Detached HEAD — pass an explicit ref (e.g. HEAD:main or v0.1.0)" >&2
    exit 1
  fi
fi

for remote in origin github; do
  if ! git remote get-url "$remote" >/dev/null 2>&1; then
    echo "Missing git remote: $remote" >&2
    exit 1
  fi
done

echo "Pushing ${REF} → origin (GitLab)"
git push origin "$REF"

echo "Pushing ${REF} → github"
git push github "$REF"

if [ "$PUSH_TAGS" -eq 1 ]; then
  echo "Pushing tags → origin"
  git push origin --tags
  echo "Pushing tags → github"
  git push github --tags
fi

echo "Dual-push complete."
