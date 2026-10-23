#!/usr/bin/env bash

set -euo pipefail

: "${RUNNER_TEMP:?RUNNER_TEMP must identify runner-local scratch storage}"
: "${GITHUB_PATH:?GITHUB_PATH must be available in GitHub Actions}"

source_dir="$RUNNER_TEMP/re2c-master-source"
build_dir="$RUNNER_TEMP/re2c-master-build"
install_dir="$RUNNER_TEMP/re2c-master"
provenance="$RUNNER_TEMP/stun-re2c-provenance.json"

git clone --depth 1 --branch master \
  https://github.com/skvadrik/re2c.git "$source_dir"
resolved_commit="$(git -C "$source_dir" rev-parse HEAD)"
[[ "$resolved_commit" =~ ^[0-9a-f]{40}$ ]]

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRE2C_BUILD_TESTS=OFF \
  -DRE2C_BUILD_RE2D=OFF \
  -DRE2C_BUILD_RE2GO=OFF \
  -DRE2C_BUILD_RE2HS=OFF \
  -DRE2C_BUILD_RE2JAVA=OFF \
  -DRE2C_BUILD_RE2JS=OFF \
  -DRE2C_BUILD_RE2OCAML=OFF \
  -DRE2C_BUILD_RE2PY=OFF \
  -DRE2C_BUILD_RE2RUST=OFF \
  -DRE2C_BUILD_RE2SWIFT=OFF \
  -DRE2C_BUILD_RE2V=OFF \
  -DRE2C_BUILD_RE2ZIG=OFF
cmake --build "$build_dir" --target re2c --parallel 2
install -D -m 0755 "$build_dir/re2c" "$install_dir/bin/re2c"

observed_version="$($install_dir/bin/re2c --version | head -n 1)"
printf '%s\n' "$install_dir/bin" >> "$GITHUB_PATH"
printf '{"repository":"skvadrik/re2c","branch":"master","commit":"%s","version":"%s"}\n' \
  "$resolved_commit" "$observed_version" > "$provenance"

printf 're2c master commit: %s\n' "$resolved_commit"
printf 're2c observed version: %s\n' "$observed_version"
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  printf '### re2c source provenance\n\n- branch: `master`\n- commit: `%s`\n- observed version: `%s`\n' \
    "$resolved_commit" "$observed_version" >> "$GITHUB_STEP_SUMMARY"
fi
