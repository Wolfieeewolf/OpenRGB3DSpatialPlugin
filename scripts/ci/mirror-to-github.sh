#!/usr/bin/env bash
# Push the current ref from GitLab to the GitHub mirror (requires GITHUB_MIRROR_TOKEN).
set -euo pipefail

if [ -z "${GITHUB_MIRROR_TOKEN:-}" ]; then
  echo "GITHUB_MIRROR_TOKEN not set — skipping GitHub mirror"
  exit 0
fi

GITHUB_REPO="${GITHUB_MIRROR_REPO:-Wolfieeewolf/OpenRGB3DSpatialPlugin}"
TARGET_BRANCH="${CI_DEFAULT_BRANCH:-main}"
REMOTE_URL="https://x-access-token:${GITHUB_MIRROR_TOKEN}@github.com/${GITHUB_REPO}.git"

git config user.email "ci@openrgb.org"
git config user.name "OpenRGB3DSpatialPlugin CI"

git remote remove github 2>/dev/null || true
git remote add github "$REMOTE_URL"

if [ -n "${CI_COMMIT_TAG:-}" ]; then
  echo "Mirroring tag ${CI_COMMIT_TAG} to GitHub"
  git push github "${CI_COMMIT_TAG}"
else
  echo "Mirroring branch ${CI_COMMIT_BRANCH} to GitHub (${TARGET_BRANCH})"
  git push github "HEAD:${TARGET_BRANCH}" --force
  git push github --tags || true
fi

echo "GitHub mirror updated: https://github.com/${GITHUB_REPO}"
