#!/usr/bin/env python3
"""Bounded, durable, single-owner inbox for an EXTERNAL GitHub App attestor.

This is a library, not a network listener, retry daemon or deployed service.
The external service must provision a private directory, verify webhooks
before enqueue, and provide its own securely stored GitHub App credentials.
There is deliberately NO automatic retry/lease reclaim after a worker crash.
A distinct authenticated operator may explicitly mark the orphaned claim
terminal FAILED with an append-only audit record; it is NEVER requeued.
"""
from contextlib import contextmanager
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sqlite3
from uuid import UUID

DIGEST = re.compile(r"[0-9a-f]{64}\Z")
MAX_EVENT_BYTES = 1024 * 1024


class InboxError(ValueError):
    """Admission/ownership/capacity failure; must not acknowledge as queued."""


@dataclass(frozen=True)
class WorkItem:
    digest: str
    event: dict
    owner: str


class DurableWebhookInbox:
    """One SQLite WAL owner-admission queue; durable only after COMMIT.

    Enqueue is compatible with attestor_webhook.admit_signed_delivery.
    Never call it before HMAC verification. The DB path must be an absolute
    file inside a private, externally provisioned directory (not a secret
    in the repository). Completed records remain until explicit maintenance.
    """

    def __init__(self, path, *, max_events=256, max_deliveries=1024):
        file = Path(path)
        if not file.is_absolute() or file.is_symlink() or not file.parent.is_dir():
            raise InboxError("inbox needs a trusted absolute, nonsymlink file path")
        if (not isinstance(max_events, int) or isinstance(max_events, bool)
                or not isinstance(max_deliveries, int)
                or isinstance(max_deliveries, bool)
                or not (1 <= max_events <= 4096)
                or not (max_events <= max_deliveries <= 16384)):
            raise InboxError("invalid bounded storage quotas")
        self.max_events = max_events
        self.max_deliveries = max_deliveries
        self.conn = sqlite3.connect(str(file), isolation_level=None, timeout=3.0)
        try:
            self.conn.execute("PRAGMA foreign_keys=ON")
            mode = self.conn.execute("PRAGMA journal_mode=WAL").fetchone()[0]
            self.conn.execute("PRAGMA synchronous=FULL")
            self.conn.execute("PRAGMA busy_timeout=3000")
            if mode.lower() != "wal":
                raise InboxError("WAL durability is required")
            version = self.conn.execute("PRAGMA user_version").fetchone()[0]
            if version not in (0, 1, 2):
                raise InboxError("unsupported inbox schema; no fallback")
            self.conn.execute("""CREATE TABLE IF NOT EXISTS work (
                seq INTEGER PRIMARY KEY AUTOINCREMENT,
                digest TEXT NOT NULL UNIQUE,
                event_json TEXT NOT NULL,
                state TEXT NOT NULL CHECK (
                    state IN ('pending','claimed','completed','failed')
                ),
                owner TEXT,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )""")
            self.conn.execute("""CREATE TABLE IF NOT EXISTS deliveries (
                delivery_id TEXT PRIMARY KEY,
                digest TEXT NOT NULL REFERENCES work(digest),
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )""")
            # An operator may explicitly terminalize a crash-abandoned
            # claim, but cannot reset it to pending or auto-retry it.
            # Version 2 is an additive, idempotent migration from v1.
            self.conn.execute("""CREATE TABLE IF NOT EXISTS operator_quarantine (
                seq INTEGER PRIMARY KEY AUTOINCREMENT,
                digest TEXT NOT NULL REFERENCES work(digest),
                claimed_owner TEXT NOT NULL,
                operator TEXT NOT NULL,
                reason TEXT NOT NULL,
                evidence_ref TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )""")
            self.conn.execute("""CREATE TRIGGER IF NOT EXISTS no_quarantine_update
                BEFORE UPDATE ON operator_quarantine
                BEGIN SELECT RAISE(ABORT, 'quarantine audit is append-only'); END
            """)
            self.conn.execute("""CREATE TRIGGER IF NOT EXISTS no_quarantine_delete
                BEFORE DELETE ON operator_quarantine
                BEGIN SELECT RAISE(ABORT, 'quarantine audit is append-only'); END
            """)
            if version != 2:
                self.conn.execute("PRAGMA user_version=2")
        except Exception:
            self.conn.close()
            raise

    @contextmanager
    def _transaction(self):
        self.conn.execute("BEGIN IMMEDIATE")
        try:
            yield
            self.conn.execute("COMMIT")
        except BaseException:
            self.conn.execute("ROLLBACK")
            raise

    def enqueue(self, delivery_id, digest, normalized_event):
        """Atomic, bounded callback; returns bool only after durable commit."""
        try:
            delivery_id = str(UUID(delivery_id))
        except (TypeError, ValueError, AttributeError) as exc:
            raise InboxError("invalid delivery id") from exc
        if not isinstance(digest, str) or DIGEST.fullmatch(digest) is None:
            raise InboxError("invalid signed body digest")
        if not isinstance(normalized_event, dict):
            raise InboxError("invalid normalized authenticated event")
        try:
            encoded = json.dumps(
                normalized_event, ensure_ascii=False, sort_keys=True,
                separators=(",", ":"), allow_nan=False
            )
        except (TypeError, ValueError) as exc:
            raise InboxError("malformed event payload") from exc
        if len(encoded.encode("utf-8")) > MAX_EVENT_BYTES:
            raise InboxError("event exceeds the inbox size limit")

        with self._transaction():
            existing = self.conn.execute(
                "SELECT digest FROM deliveries WHERE delivery_id=?",
                (delivery_id,)
            ).fetchone()
            if existing is not None:
                if existing[0] != digest:
                    raise InboxError("delivery UUID reused for different payload")
                return False

            total_deliveries = self.conn.execute(
                "SELECT COUNT(*) FROM deliveries"
            ).fetchone()[0]
            if total_deliveries >= self.max_deliveries:
                raise InboxError("delivery capacity exhausted; no silent eviction")

            existing_body = self.conn.execute(
                "SELECT event_json FROM work WHERE digest=?", (digest,)
            ).fetchone()
            if existing_body is not None:
                if existing_body[0] != encoded:
                    raise InboxError("body digest collision / event mismatch")
                self.conn.execute(
                    "INSERT INTO deliveries(delivery_id,digest) VALUES (?,?)",
                    (delivery_id, digest)
                )
                return False

            total_events = self.conn.execute(
                "SELECT COUNT(*) FROM work"
            ).fetchone()[0]
            if total_events >= self.max_events:
                raise InboxError("inbox event capacity exhausted")
            self.conn.execute(
                "INSERT INTO work(digest,event_json,state) VALUES (?,?,'pending')",
                (digest, encoded)
            )
            self.conn.execute(
                "INSERT INTO deliveries(delivery_id,digest) VALUES (?,?)",
                (delivery_id, digest)
            )
            return True

    def claim_next(self, owner):
        """At most ONE running item across all processes; no auto reclaim."""
        if not isinstance(owner, str) or not owner or len(owner) > 128:
            raise InboxError("explicit bounded worker owner required")
        with self._transaction():
            active = self.conn.execute(
                "SELECT digest FROM work WHERE state='claimed' LIMIT 1"
            ).fetchone()
            if active is not None:
                return None
            pending = self.conn.execute(
                "SELECT digest,event_json FROM work "
                "WHERE state='pending' ORDER BY seq LIMIT 1"
            ).fetchone()
            if pending is None:
                return None
            digest, raw_event = pending
            self.conn.execute(
                "UPDATE work SET state='claimed',owner=? "
                "WHERE digest=? AND state='pending'", (owner, digest)
            )
            return WorkItem(digest=digest, event=json.loads(raw_event), owner=owner)

    def complete(self, digest, owner, *, successful):
        """Explicit terminal transition. Failed claims are NOT auto-retried."""
        if (not isinstance(digest, str) or DIGEST.fullmatch(digest) is None
                or not isinstance(owner, str) or not owner
                or type(successful) is not bool):
            raise InboxError("invalid terminal claim")
        state = "completed" if successful else "failed"
        with self._transaction():
            cursor = self.conn.execute(
                "UPDATE work SET state=? WHERE digest=? "
                "AND state='claimed' AND owner=?", (state, digest, owner)
            )
            if cursor.rowcount != 1:
                raise InboxError("worker does not own the claimed envelope")

    def quarantine_claim(self, digest, claimed_owner, *,
                         operator, reason, evidence_ref):
        """Irreversibly fail an operator-reviewed orphaned claim, no retry.

        This method does NOT authenticate the caller: the external deployment
        must restrict it to its separate, audited operator identity. The
        evidence reference must point to an independently inspected live
        status/CI record; the API does not trust arbitrary webhook evidence.
        """
        if (not isinstance(digest, str) or DIGEST.fullmatch(digest) is None or
                not isinstance(claimed_owner, str) or
                not claimed_owner.strip() or len(claimed_owner) > 128 or
                not isinstance(operator, str) or
                not operator.strip() or len(operator) > 128 or
                operator == claimed_owner or
                not isinstance(reason, str) or
                not (16 <= len(reason.strip()) <= 512) or
                not isinstance(evidence_ref, str) or
                not (6 <= len(evidence_ref.strip()) <= 256)):
            raise InboxError("explicit bounded operator/claim/reason/evidence required")
        with self._transaction():
            record = self.conn.execute(
                "SELECT state,owner FROM work WHERE digest=?", (digest,)
            ).fetchone()
            if record is None or record != ("claimed", claimed_owner):
                raise InboxError("claim no longer belongs to expected owner")
            updated = self.conn.execute(
                "UPDATE work SET state='failed' WHERE digest=? "
                "AND state='claimed' AND owner=?", (digest, claimed_owner)
            )
            if updated.rowcount != 1:
                raise InboxError("lost terminal-claim arbitration")
            self.conn.execute(
                "INSERT INTO operator_quarantine("
                "digest,claimed_owner,operator,reason,evidence_ref)"
                " VALUES (?,?,?,?,?)",
                (digest, claimed_owner, operator, reason.strip(),
                 evidence_ref.strip())
            )
        return True

    def quarantine_log(self, digest):
        """Read the retained append-only operator record for a work digest."""
        if not isinstance(digest, str) or DIGEST.fullmatch(digest) is None:
            raise InboxError("invalid quarantine digest")
        rows = self.conn.execute(
            "SELECT digest,claimed_owner,operator,reason,evidence_ref,created_at "
            "FROM operator_quarantine WHERE digest=? ORDER BY seq", (digest,)
        ).fetchall()
        return [
            {"digest": row[0], "claimed_owner": row[1], "operator": row[2],
             "reason": row[3], "evidence_ref": row[4], "created_at": row[5]}
            for row in rows
        ]

    def summary(self):
        counts = dict(self.conn.execute(
            "SELECT state,COUNT(*) FROM work GROUP BY state"
        ).fetchall())
        return {
            "pending": counts.get("pending", 0),
            "claimed": counts.get("claimed", 0),
            "completed": counts.get("completed", 0),
            "failed": counts.get("failed", 0),
            "deliveries": self.conn.execute(
                "SELECT COUNT(*) FROM deliveries"
            ).fetchone()[0],
        }

    def close(self):
        self.conn.close()
