#!/usr/bin/env bash
# Scope PR synchronize to the latest update, initial PRs to base-to-head,
# and master pushes to the exact event before/after source delta.
# Fail OPEN for missing or invalid metadata, git history, checkout identity,
# or file comparison. workflow_dispatch and ci/full force full qualification.
set -euo pipefail

emit() {
    printf 'affected=%s\n' "$1" >> "${GITHUB_OUTPUT:?GITHUB_OUTPUT is required}"
    printf 'CI affected: %s (%s)\n' "$1" "$2"
}

event_name="${GITHUB_EVENT_NAME:-}"
case "$event_name" in
    workflow_dispatch)
        emit true "explicit complete workflow dispatch"
        exit 0
        ;;
    pull_request|push) ;;
    *)
        emit true "unsupported workflow event: ${event_name:-unknown}"
        exit 0
        ;;
esac

if [[ ! -r "${GITHUB_EVENT_PATH:-}" ]] || ! command -v jq >/dev/null; then
    emit true "missing event payload or jq"
    exit 0
fi

if [[ "$event_name" == "push" ]]; then
    # Master pushes use their actual committed delta rather than running
    # every native suite on every unrelated change.
    before="$(jq -r '.before // empty' "$GITHUB_EVENT_PATH")"
    after="$(jq -r '.after // empty' "$GITHUB_EVENT_PATH")"
    diff_kind="master push"
else
    action="$(jq -r '.action // empty' "$GITHUB_EVENT_PATH")"
    if [[ "$action" == "labeled" ]] && \
        [[ "$(jq -r '.label.name // empty' "$GITHUB_EVENT_PATH")" != "ci/full" ]]; then
        emit false "unrelated pull request label"
        exit 0
    fi
    if jq -e '[.pull_request.labels[]?.name] | index("ci/full") != null' \
        "$GITHUB_EVENT_PATH" >/dev/null 2>&1; then
        emit true "ci/full label requires exact-HEAD requalification"
        exit 0
    fi

    case "$action" in
        synchronize)
            before="$(jq -r '.before // empty' "$GITHUB_EVENT_PATH")"
            after="$(jq -r '.after // .pull_request.head.sha // empty' "$GITHUB_EVENT_PATH")"
            diff_kind="latest PR push"
            ;;
        opened|reopened|ready_for_review)
            before="$(jq -r '.pull_request.base.sha // empty' "$GITHUB_EVENT_PATH")"
            after="$(jq -r '.pull_request.head.sha // empty' "$GITHUB_EVENT_PATH")"
            diff_kind="pull request"
            ;;
        labeled)
            emit true "ci/full label requires exact-HEAD requalification"
            exit 0
            ;;
        *)
            emit true "unsupported pull request action: ${action:-unknown}"
            exit 0
            ;;
    esac
fi
sha='^[0-9a-fA-F]{40}$'
if [[ ! "$before" =~ $sha || ! "$after" =~ $sha ]] || \
    [[ "$before" =~ ^0+$ || "$after" =~ ^0+$ ]]; then
    emit true "missing or invalid before/after SHA"
    exit 0
fi

# The checkout must be the exact event after/head SHA, not a synthetic merge ref.
if [[ "$(git rev-parse HEAD 2>/dev/null || :)" != "$after" ]]; then
    emit true "checked-out HEAD differs from pull request head"
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

# A leading ! excludes only that exact file scope, useful for cheap
# parser-only validation: a different changed file still triggers the gate.
# The parser smoke tests stay mandatory for excluded parser sources.
has_inclusion=false
for glob in "$@"; do
    if [[ "$glob" != '!'* ]]; then
        has_inclusion=true
        break
    fi
done
if [[ "$has_inclusion" != true ]]; then
    emit true "no positive scope supplied"
    exit 0
fi

while IFS= read -r -d '' filename; do
    excluded=false
    for glob in "$@"; do
        if [[ "$glob" == '!'* && "$filename" == ${glob:1} ]]; then
            excluded=true
            break
        fi
    done
    if [[ "$excluded" == true ]]; then
        continue
    fi

    for glob in "$@"; do
        [[ "$glob" == '!'* ]] && continue
        # RHS intentionally acts as a Bash glob (including directory slashes).
        if [[ "$filename" == $glob ]]; then
            emit true "$diff_kind changed an applicable file"
            exit 0
        fi
    done
done < "$changed_paths"

emit false "$diff_kind does not touch this workflow's scope"
