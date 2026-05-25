"""
USSD Application Server for the Kinyarwanda AI Platform.

Handles incoming USSD requests from Africa's Talking gateway (or any
gateway that POSTs the AT USSD webhook format).  No internet is required
on the user's side — this runs on a server; callers use any GSM phone.

Environment variables (see .env.example):
    AT_API_KEY      Africa's Talking API key
    AT_USERNAME     Africa's Talking username
    OPENROUTER_KEY  Optional: key for cloud LLM fallback
    SECRET_KEY      Flask session secret (random string)
    PORT            Port to listen on (default 5000)
    DEBUG           Set to "1" for development logging
"""

import os
import logging
from flask import Flask, request, Response
from session import USSDSession
from menus import (
    MENU_HOME, MENU_HELP, build_response, truncate_ussd
)
from ai_bridge import KinyarwandaAI

logging.basicConfig(
    level=logging.DEBUG if os.getenv("DEBUG") == "1" else logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s"
)
log = logging.getLogger("ussd.server")

app = Flask(__name__)
app.secret_key = os.getenv("SECRET_KEY", "change-me-in-production")

ai = KinyarwandaAI()
sessions = USSDSession()


@app.route("/ussd", methods=["POST"])
def ussd_callback():
    """
    Africa's Talking posts these fields on every USSD interaction:
        sessionId   — unique per dial session (reused across pages)
        serviceCode — the short code dialled, e.g. *290#
        phoneNumber — caller's MSISDN e.g. +250788123456
        networkCode — telecom code (MTN=63902, Airtel=62120 in Rwanda)
        text        — accumulated user input, e.g. "1*2*Amategeko"
    """
    session_id   = request.form.get("sessionId", "")
    service_code = request.form.get("serviceCode", "")
    phone        = request.form.get("phoneNumber", "")
    network      = request.form.get("networkCode", "")
    text         = request.form.get("text", "")

    log.info("USSD | session=%s phone=%s text=%r", session_id, phone, text)

    response_text = handle_ussd(session_id, phone, text)
    return Response(response_text, mimetype="text/plain")


def handle_ussd(session_id: str, phone: str, text: str) -> str:
    """Route the user's input to the right handler, return USSD response."""
    parts = [p.strip() for p in text.split("*")] if text else [""]

    # ── First contact ──────────────────────────────────────────────────────
    if text == "":
        sessions.new(session_id, phone)
        return build_response(MENU_HOME, end=False)

    # ── Top-level menu choice ──────────────────────────────────────────────
    depth = len(parts)
    choice = parts[0]

    if depth == 1:
        return _handle_main_menu(session_id, choice)

    if depth == 2:
        return _handle_submenu(session_id, parts[0], parts[1])

    if depth >= 3:
        return _handle_input(session_id, parts[0], parts[1], "*".join(parts[2:]))

    return build_response("Habari. Subira aho wari uhagaze.\n0. Rudi", end=False)


# ──────────────────────────────────────────────────────────────────────────
# Main menu handlers
# ──────────────────────────────────────────────────────────────────────────

def _handle_main_menu(session_id: str, choice: str) -> str:
    if choice == "1":
        return build_response(
            "SOBANURA INYANDIKO\n"
            "Andika ubutumwa cyangwa ikibazo:\n"
            "(Urugero: Amategeko y'umuhanda)\n",
            end=False
        )
    if choice == "2":
        return build_response(
            "IBYEREKEYE LETA\n"
            "1. Uburyo bwo kwiyandikisha\n"
            "2. Amazina ya serivisi\n"
            "3. Uburenganzira bw'umwenegihugu\n"
            "0. Subira",
            end=False
        )
    if choice == "3":
        return build_response(
            "UBUZIMA\n"
            "1. Ubuzima bw'umwana\n"
            "2. Indwara z'ahantu\n"
            "3. Inshuti z'ubuzima\n"
            "0. Subira",
            end=False
        )
    if choice == "4":
        return build_response(
            "UBUREZI\n"
            "1. Amashuri abanza\n"
            "2. Amashuri yisumbuye\n"
            "3. Kaminuza\n"
            "4. Ibibazo by'isomo\n"
            "0. Subira",
            end=False
        )
    if choice == "5":
        return build_response(
            "IKORANABUHANGA\n"
            "1. Ibyuma bya interineti\n"
            "2. Uburyo bwo gukorana na telefone\n"
            "3. Gukoresha imari ya telefone\n"
            "0. Subira",
            end=False
        )
    if choice == "0":
        sessions.clear(session_id)
        return build_response("Murakoze gukoresha serivisi ya Kinyarwanda AI.\nMuhora mutugezeho!", end=True)

    return build_response(MENU_HOME, end=False)


def _handle_submenu(session_id: str, main: str, sub: str) -> str:
    """Handle second-level selections where the user picks a topic."""
    topic_map = {
        ("2", "1"): "Uburyo bwo kwiyandikisha mu Rwanda",
        ("2", "2"): "Amazina ya serivisi za leta mu Rwanda",
        ("2", "3"): "Uburenganzira bw'umwenegihugu mu Rwanda",
        ("3", "1"): "Ubuzima bw'umwana munsi y'imyaka itanu",
        ("3", "2"): "Indwara z'ahantu mu Rwanda",
        ("3", "3"): "Inshuti z'ubuzima mu Rwanda",
        ("4", "1"): "Gahunda y'amashuri abanza mu Rwanda",
        ("4", "2"): "Gahunda y'amashuri yisumbuye mu Rwanda",
        ("4", "3"): "Gahunda ya kaminuza mu Rwanda",
        ("4", "4"): "Ibibazo by'isomo - bite wasobanura?",
        ("5", "1"): "Interineti ikorana gute?",
        ("5", "2"): "Gukoresha telefone mu buryo bwiza",
        ("5", "3"): "Imari ya telefone - Mobile Money ikorana gute?",
    }

    if sub == "0":
        return build_response(MENU_HOME, end=False)

    topic = topic_map.get((main, sub))
    if not topic:
        return build_response(MENU_HOME, end=False)

    sessions.set_topic(session_id, topic)
    return build_response(
        f"Sobanura: {topic[:60]}\n\n"
        "Andika: 1. Incamake (짧)\n"
        "        2. Ibisobanuro birambuye\n"
        "        3. Urugero rw'ibikorwa\n"
        "0. Subira",
        end=False
    )


def _handle_input(session_id: str, main: str, sub: str, user_text: str) -> str:
    """
    Third level:
      - If main=="1" (free text query): user_text is their actual question
      - Otherwise: sub is the style choice (1/2/3) for a pre-set topic
    """
    if main == "1":
        # Free-text query path
        if not user_text.strip():
            return build_response("Andika ikibazo cyawe hanyuma ugaragaze.", end=False)
        return _ai_query(session_id, user_text.strip(), style="summary")

    # Topic-based path: sub is topic choice, user_text is style choice
    style_map = {"1": "summary", "2": "detailed", "3": "example"}
    style = style_map.get(sub, "summary")
    topic = sessions.get_topic(session_id)

    if not topic:
        return build_response(MENU_HOME, end=False)

    return _ai_query(session_id, topic, style=style)


def _ai_query(session_id: str, question: str, style: str) -> str:
    """Call the AI bridge and format the answer for USSD."""
    try:
        answer = ai.explain(question, style=style)
        # USSD messages are capped at 182 characters per page.
        # For longer answers we show the first page with a continue option.
        pages = _paginate(answer, 160)
        sessions.set_pages(session_id, pages)
        return _serve_page(session_id, 0)
    except Exception as exc:
        log.error("AI query failed: %s", exc)
        return build_response(
            "Ikibazo cyabaye mu gutanga igisubizo.\n"
            "Gerageza nyuma gato.\n0. Rudi",
            end=False
        )


def _paginate(text: str, page_size: int = 160) -> list:
    """Split a long response into USSD-sized pages."""
    words = text.split()
    pages, current = [], ""
    for word in words:
        if len(current) + len(word) + 1 > page_size:
            pages.append(current.strip())
            current = word + " "
        else:
            current += word + " "
    if current.strip():
        pages.append(current.strip())
    return pages or ["Nta gisubizo kibonetse."]


def _serve_page(session_id: str, page_idx: int) -> str:
    pages = sessions.get_pages(session_id)
    if not pages:
        return build_response(MENU_HOME, end=False)

    text = pages[page_idx]
    has_next = page_idx + 1 < len(pages)

    if has_next:
        nav = f"\n[{page_idx+1}/{len(pages)}] 1.Komeza 0.Rudi"
    else:
        nav = "\n\n1.Ikibazo kindi 0.Menu ntoya"

    return build_response(text + nav, end=False)


@app.route("/ussd/page", methods=["POST"])
def ussd_page_nav():
    """Handles pagination navigation (next page / back)."""
    session_id = request.form.get("sessionId", "")
    text = request.form.get("text", "")
    parts = [p.strip() for p in text.split("*")] if text else [""]

    last = parts[-1] if parts else ""
    page = sessions.get_page_idx(session_id)

    if last == "1":
        new_page = page + 1
        sessions.set_page_idx(session_id, new_page)
        return Response(_serve_page(session_id, new_page), mimetype="text/plain")

    return Response(build_response(MENU_HOME, end=False), mimetype="text/plain")


@app.route("/health", methods=["GET"])
def health():
    return {"status": "ok", "service": "kinyarwanda-ussd"}, 200


if __name__ == "__main__":
    port = int(os.getenv("PORT", 5000))
    app.run(host="0.0.0.0", port=port, debug=os.getenv("DEBUG") == "1")
