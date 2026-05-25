"""
USSD Session Manager.

Stores per-session state in memory (suitable for a single-server deployment).
For multi-server deployments, swap _store for a Redis backend by replacing
_get / _set / _del with redis.get / redis.setex / redis.delete.

Session lifetime: USSD sessions on MTN/Airtel Rwanda time out after 3 minutes
of inactivity at the telecom level.  We keep sessions for 10 minutes on our
side to handle edge cases.
"""

import time
import threading
from typing import Any, Optional

_SESSION_TTL = 600  # seconds


class USSDSession:
    def __init__(self):
        self._store: dict[str, dict] = {}
        self._lock = threading.Lock()
        self._start_gc()

    # ── Public API ────────────────────────────────────────────────────────

    def new(self, session_id: str, phone: str) -> None:
        with self._lock:
            self._store[session_id] = {
                "phone": phone,
                "topic": None,
                "pages": [],
                "page_idx": 0,
                "created": time.time(),
                "touched": time.time(),
            }

    def clear(self, session_id: str) -> None:
        with self._lock:
            self._store.pop(session_id, None)

    def set_topic(self, session_id: str, topic: str) -> None:
        self._update(session_id, topic=topic)

    def get_topic(self, session_id: str) -> Optional[str]:
        return self._get(session_id, "topic")

    def set_pages(self, session_id: str, pages: list) -> None:
        self._update(session_id, pages=pages, page_idx=0)

    def get_pages(self, session_id: str) -> list:
        return self._get(session_id, "pages") or []

    def set_page_idx(self, session_id: str, idx: int) -> None:
        self._update(session_id, page_idx=idx)

    def get_page_idx(self, session_id: str) -> int:
        return self._get(session_id, "page_idx") or 0

    # ── Internal helpers ──────────────────────────────────────────────────

    def _get(self, session_id: str, key: str) -> Any:
        with self._lock:
            sess = self._store.get(session_id)
            if sess:
                sess["touched"] = time.time()
                return sess.get(key)
        return None

    def _update(self, session_id: str, **kwargs) -> None:
        with self._lock:
            if session_id not in self._store:
                self._store[session_id] = {"touched": time.time()}
            self._store[session_id].update(kwargs)
            self._store[session_id]["touched"] = time.time()

    def _start_gc(self):
        """Background thread: evict sessions older than TTL."""
        def gc():
            while True:
                time.sleep(60)
                cutoff = time.time() - _SESSION_TTL
                with self._lock:
                    expired = [
                        k for k, v in self._store.items()
                        if v.get("touched", 0) < cutoff
                    ]
                    for k in expired:
                        del self._store[k]

        t = threading.Thread(target=gc, daemon=True)
        t.start()
