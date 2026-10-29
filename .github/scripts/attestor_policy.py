#!/usr/bin/env python3
"""Immutable release-bundle identity for the EXTERNAL attestation worker.

The expected SHA256 comes ONLY from an independent, operator-approved
deployment configuration. Never trust a digest supplied by a PR, a webhook,
a runtime environment variable written by Actions, or the bundle itself.
"""
import hashlib
import re
from pathlib import Path

from ci_required_matrix import PUBLIC, RUNTIME, WORKFLOWS

MAX_POLICY_FILE_BYTES = 2 * 1024 * 1024
POLICY_FILES = tuple(sorted(set((
    ".github/scripts/attestor_policy.py",
    ".github/scripts/attestor_webhook.py",
    ".github/scripts/attestor_inbox.py",
    ".github/scripts/attestor_worker.py",
    ".github/scripts/ci_required_matrix.py",
    ".github/scripts/verify_exact_head_ci.py",
    ".github/scripts/publish_required_acceptance.py",
    PUBLIC, RUNTIME,
) + tuple(".github/workflows/" + name for name in WORKFLOWS))))
DIGEST_PATTERN = re.compile(r"[0-9a-f]{64}\Z")


class PolicyIntegrityError(ValueError):
    """Refuse to claim work if the trusted deployment cannot be identified."""


def _trusted_root(root):
    path = Path(root)
    if not path.is_absolute() or not path.is_dir() or path.is_symlink():
        raise PolicyIntegrityError("policy root must be a real absolute directory")
    # No symlinks in the root's ancestor chain or underneath the root.
    if path.resolve(strict=True) != path:
        raise PolicyIntegrityError("policy root resolves through a symlink")
    return path


def _trusted_file(root, name):
    candidate = root / name
    parent = candidate.parent
    while parent != root:
        if parent.is_symlink() or not parent.is_dir():
            raise PolicyIntegrityError("policy bundle contains a symlinked directory")
        if root not in parent.parents:
            raise PolicyIntegrityError("policy path escaped the pinned root")
        parent = parent.parent
    if candidate.is_symlink() or not candidate.is_file():
        raise PolicyIntegrityError("missing or linked policy file: " + name)
    length = candidate.stat().st_size
    if length <= 0 or length > MAX_POLICY_FILE_BYTES:
        raise PolicyIntegrityError("missing, empty or oversized policy file: " + name)
    data = candidate.read_bytes()
    if len(data) != length:
        raise PolicyIntegrityError("policy file mutated while reading: " + name)
    return data


def policy_bundle_sha256(root):
    """Deterministic SHA256 of exact pathname+file-length+bytes, never glob PR.

    The digest alone cannot make mutable storage safe. The external launcher
    MUST deploy an immutable/read-only approved release and supply the
    expected digest out of band, then keep the same immutable mount for
    the lifetime of each worker process.
    """
    base = _trusted_root(root)
    hash_state = hashlib.sha256(b"stun-external-attestor-policy-v1\x00")
    for relative_path in POLICY_FILES:
        payload = _trusted_file(base, relative_path)
        name_bytes = relative_path.encode("utf-8")
        hash_state.update(len(name_bytes).to_bytes(4, "big"))
        hash_state.update(name_bytes)
        hash_state.update(len(payload).to_bytes(8, "big"))
        hash_state.update(payload)
    return hash_state.hexdigest()


def verify_policy_bundle(root, expected_digest, *, loaded_module_paths=None):
    if (not isinstance(expected_digest, str) or
            DIGEST_PATTERN.fullmatch(expected_digest) is None):
        raise PolicyIntegrityError("explicit 64-character pinned digest required")
    base = _trusted_root(root)
    if loaded_module_paths is not None:
        for module_name, actual_path in loaded_module_paths.items():
            if module_name not in POLICY_FILES:
                raise PolicyIntegrityError("unexpected loaded policy module")
            actual = Path(actual_path)
            if not actual.is_absolute() or actual.resolve(strict=True) != base / module_name:
                raise PolicyIntegrityError("runtime module is not from pinned bundle: " + module_name)
    actual = policy_bundle_sha256(base)
    if not __import__("hmac").compare_digest(actual, expected_digest):
        raise PolicyIntegrityError("trusted policy release digest mismatch")
    return actual
