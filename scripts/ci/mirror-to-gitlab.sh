#!/usr/bin/env bash
# Push the current GitHub ref to the GitLab mirror (requires GITLAB_MIRROR_TOKEN).
set -euo pipefail

if [ -z "${GITLAB_MIRROR_TOKEN:-}" ]; then
  echo "GITLAB_MIRROR_TOKEN not set — skipping GitLab mirror"
  exit 0
fi

GITLAB_REPO="${GITLAB_MIRROR_REPO:-wolfieeewolf1/OpenRGB3DSpatialPlugin}"
TARGET_BRANCH="${GITLAB_MIRROR_BRANCH:-main}"
REMOTE_URL="https://oauth2:${GITLAB_MIRROR_TOKEN}@gitlab.com/${GITLAB_REPO}.git"

git config user.email "ci@openrgb.org"
git config user.name "OpenRGB3DSpatialPlugin CI"

git remote remove gitlab-mirror 2>/dev/null || true
git remote add gitlab-mirror "$REMOTE_URL"

if [[ "${GITHUB_REF:-}" == refs/tags/* ]]; then
  TAG_NAME="${GITHUB_REF#refs/tags/}"
  echo "Mirroring tag ${TAG_NAME} to GitLab"
  git push gitlab-mirror "refs/tags/${TAG_NAME}:refs/tags/${TAG_NAME}"
else
  echo "Mirroring HEAD to GitLab ${TARGET_BRANCH}"
  git push gitlab-mirror "HEAD:${TARGET_BRANCH}" --force
  git push gitlab-mirror --tags || true
fi

echo "GitLab mirror updated: https://gitlab.com/${GITLAB_REPO}"
