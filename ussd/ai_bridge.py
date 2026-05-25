"""
AI Bridge — connects USSD sessions to the Kinyarwanda AI engine.

Explanation strategy (in priority order):
  1. Local knowledge base (fast, offline, no API cost) — answers for
     common Rwanda government/health/education topics stored in JSON.
  2. NLP engine (C library via subprocess) — validates and normalises
     both the question and the answer.
  3. Cloud LLM fallback (OpenRouter → Claude/GPT-4o) — used only when
     the local KB does not cover the question.  Requires OPENROUTER_KEY.

All answers are returned in Kinyarwanda and are kept short enough to
fit inside 2–3 USSD pages (≈ 400 characters before pagination).
"""

import os
import json
import subprocess
import logging
from pathlib import Path
from typing import Optional

log = logging.getLogger("ussd.ai_bridge")

KB_PATH = Path(__file__).parent / "knowledge_base.json"

STYLE_PROMPTS = {
    "summary": (
        "Sobanura mu magambo make cyane (imirongo 2-3 gusa) mu Kinyarwanda: "
    ),
    "detailed": (
        "Sobanura neza kandi birambuye mu Kinyarwanda (imirongo 5-7): "
    ),
    "example": (
        "Tanga urugero rw'ibikorwa bya buri munsi busobanura mu Kinyarwanda: "
    ),
}

NLP_BINARY = Path(__file__).parent.parent / "kinyarwanda_nlp"


class KinyarwandaAI:
    def __init__(self):
        self._kb = self._load_kb()
        self._has_nlp = NLP_BINARY.exists()
        self._openrouter_key = os.getenv("OPENROUTER_KEY", "")
        if not self._openrouter_key:
            log.warning("OPENROUTER_KEY not set — cloud fallback disabled")

    # ── Public API ────────────────────────────────────────────────────────

    def explain(self, question: str, style: str = "summary") -> str:
        """
        Return a Kinyarwanda explanation of `question`.
        style: "summary" | "detailed" | "example"
        """
        # 1. Local knowledge base
        answer = self._kb_lookup(question)
        if answer:
            return self._apply_style(answer, style)

        # 2. Cloud LLM (requires internet on the SERVER side only)
        if self._openrouter_key:
            return self._cloud_explain(question, style)

        # 3. Graceful degradation — still useful offline
        return self._offline_fallback(question)

    # ── Knowledge base ────────────────────────────────────────────────────

    def _load_kb(self) -> dict:
        if KB_PATH.exists():
            try:
                with open(KB_PATH, encoding="utf-8") as f:
                    return json.load(f)
            except Exception as exc:
                log.error("Failed to load knowledge base: %s", exc)
        return {}

    def _kb_lookup(self, question: str) -> Optional[str]:
        """Fuzzy keyword match against the local knowledge base."""
        q_lower = question.lower()
        best_score, best_answer = 0, None
        for entry in self._kb.get("entries", []):
            keywords = entry.get("keywords", [])
            score = sum(1 for kw in keywords if kw.lower() in q_lower)
            if score > best_score:
                best_score = score
                best_answer = entry.get("answer_rw", "")
        # Require at least one keyword to match
        return best_answer if best_score >= 1 else None

    def _apply_style(self, full_answer: str, style: str) -> str:
        if style == "summary":
            sentences = full_answer.split(".")
            return ". ".join(sentences[:2]).strip() + "."
        if style == "example":
            # Return the last paragraph which typically has examples
            paras = [p.strip() for p in full_answer.split("\n") if p.strip()]
            return paras[-1] if paras else full_answer
        return full_answer  # detailed — return everything

    # ── Cloud fallback ────────────────────────────────────────────────────

    def _cloud_explain(self, question: str, style: str) -> str:
        try:
            import urllib.request
            import urllib.error

            style_prefix = STYLE_PROMPTS.get(style, STYLE_PROMPTS["summary"])
            prompt = (
                f"{style_prefix}{question}\n\n"
                "Igisubizo kigomba kuba:\n"
                "- Mu Kinyarwanda gusa\n"
                "- Kigufi (amagambo 60-80 gusa)\n"
                "- Byoroshye gusobanukirwa\n"
                "- Nta magambo y'Icyongereza"
            )

            payload = json.dumps({
                "model": "anthropic/claude-haiku-4-5-20251001",
                "messages": [
                    {
                        "role": "system",
                        "content": (
                            "Uri inzobere mu Kinyarwanda. Subiza GUSA mu Kinyarwanda. "
                            "Ibisubizo byawe bigomba kuba ngufi, byoroshye, kandi bigendana "
                            "n'abantu basanzwe, harimo n'abatize. "
                            "Ntukoreshe amagambo y'amahanga keretse nta kintu gifatika kirimo."
                        ),
                    },
                    {"role": "user", "content": prompt},
                ],
                "max_tokens": 300,
                "temperature": 0.3,
            }).encode("utf-8")

            req = urllib.request.Request(
                "https://openrouter.ai/api/v1/chat/completions",
                data=payload,
                headers={
                    "Content-Type": "application/json",
                    "Authorization": f"Bearer {self._openrouter_key}",
                    "HTTP-Referer": "https://github.com/josep-prog/kinyarwanda_nlp",
                    "X-Title": "Kinyarwanda AI USSD",
                },
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=8) as resp:
                data = json.loads(resp.read().decode("utf-8"))
                answer = data["choices"][0]["message"]["content"].strip()
                return self._nlp_validate(answer)

        except Exception as exc:
            log.error("Cloud LLM call failed: %s", exc)
            return self._offline_fallback(question)

    # ── NLP validation ────────────────────────────────────────────────────

    def _nlp_validate(self, text: str) -> str:
        """
        Run the C NLP engine to spell-correct the AI output.
        Falls back to the raw text if the binary is unavailable.
        """
        if not self._has_nlp:
            return text
        try:
            result = subprocess.run(
                [str(NLP_BINARY), "--correct", text],
                capture_output=True, text=True, timeout=3
            )
            corrected = result.stdout.strip()
            return corrected if corrected else text
        except Exception as exc:
            log.warning("NLP validation skipped: %s", exc)
            return text

    # ── Offline fallback ──────────────────────────────────────────────────

    def _offline_fallback(self, question: str) -> str:
        """
        When both the KB and the cloud are unavailable, give the user
        a helpful Kinyarwanda message rather than a cryptic error.
        """
        return (
            "Ikibazo cyawe nticyansubijwa ubu.\n"
            "Gerageza izi serivisi:\n"
            "• Leta: 3883 (MTN)\n"
            "• RURA: 3939\n"
            "• Ubuzima: 114\n"
            "Murakoze."
        )
