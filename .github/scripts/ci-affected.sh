#!/usr/bin/env bash
# Evaluate ONLY the files changed by the latest pull_request:synchronize update.
# PR-level paths filters include the cumulative diff against master and can
# re-run unrelated expensive suites on every commit of a long-lived PR.
#
# Fail OPEN (run the check) when GitHub event metadata, history or diff is
# unavailable. On opened/reopened/ready_for_review, workflow_dispatch, push,
# or an opt-in ci/full label, always run the complete applicable gate.
set -euo pipefail

emit() {
    printf 'affected=%s\n' "$1" >> "${GITHUB_OUTPUT:?GITHUB_OUTPUT is required}"
    printf 'CI affected: %s (%s)\n' "$1" "$2"
}

if [[ "${GITHUB_EVENT_NAME:-}" != "pull_request" ]]; then
    emit true "not a pull_request update"
    exit 0
fi

if [[ ! -r "${GITHUB_EVENT_PATH:-}" ]] || ! command -v jq >/dev/null; then
    emit true "missing event payload or jq"
    exit 0
fi

action="$(jq -r '.action // empty' "$GITHUB_EVENT_PATH")"
if [[ "$action" != "synchronize" ]]; then
    emit true "pull request action: ${action:-unknown}"
    exit 0
fi

if jq -e '[.pull_request.labels[]?.name] | index("ci/full") != null' \
    "$GITHUB_EVENT_PATH" >/dev/null 2>&1; then
    emit true "ci/full label requires exact-HEAD requalification"
    exit 0
fi

before="$(jq -r '.before // empty' "$GITHUB_EVENT_PATH")"
after="$(jq -r '.after // .pull_request.head.sha // empty' "$GITHUB_EVENT_PATH")"
sha='^[0-9a-fA-F]{40}$'
if [[ ! "$before" =~ $sha || ! "$after" =~ $sha ]] || \
    [[ "$before" =~ ^0+$ || "$after" =~ ^0+$ ]]; then
    emit true "missing or invalid before/after SHA"
    exit 0
fi

# The checkout must be the exact PR head, not GitHub's synthetic merge ref.
if [[ "$(git rev-parse HEAD 2>/dev/null || :)" != "$after" ]]; then
    emit true "checked-out HEAD differs from synchronize.after"
    exit 0
fi
if ! git cat-file -e "$before^{commit}" 2>/dev/null; then
    if ! git fetch -q --no-tags --depth=1 origin "$before"; then
        emit true "previous PR head unavailable"
        exit 0
    fi
fi
if ! git cat-file -e "$before^{commit}" 2>/dev/null; then
    emit true "previous PR head cannot be resolved"
    exit 0
fi
if (( $# == 0 )); then
    emit true "no scope patterns supplied"
    exit 0
fi

changed_paths="$(mktemp)"
trap 'rm -f "$changed_paths"' EXIT
# --no-renames includes both sides of moves; -z safely handles unusual names.
if ! git diff --no-renames --name-only -z "$before" "$after" -- > "$changed_paths"; then
    emit true "could not compare PR heads"
    exit 0
fi

while IFS= read -r -d '' filename; do
    for glob in "$@"; do
        # RHS intentionally acts as a Bash glob (including directory slashes).
        if [[ "$filename" == $glob ]]; then
            emit true "latest push changed an applicable file"
            exit 0
        fi
    done
done < "$changed_paths"

emit false "latest push does not touch this workflow's scope"
