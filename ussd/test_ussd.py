"""
Unit tests for the USSD server — no internet required.

Run with:  python -m pytest test_ussd.py -v
"""

import pytest
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

# Patch environment before import
os.environ.setdefault("SECRET_KEY", "test-secret")
os.environ.setdefault("OPENROUTER_KEY", "")

from server import app, handle_ussd
from menus import build_response, truncate_ussd
from session import USSDSession


@pytest.fixture
def client():
    app.config["TESTING"] = True
    with app.test_client() as c:
        yield c


# ── Menu rendering ─────────────────────────────────────────────────────────

def test_build_response_continue():
    r = build_response("Murakaza neza!", end=False)
    assert r.startswith("CON ")
    assert "Murakaza neza!" in r


def test_build_response_end():
    r = build_response("Murakoze!", end=True)
    assert r.startswith("END ")


def test_truncate_does_not_exceed_limit():
    long_text = "a" * 200
    result = truncate_ussd(long_text, limit=178)
    assert len(result) <= 178


def test_truncate_short_text_unchanged():
    short = "Muraho"
    assert truncate_ussd(short) == short


# ── Session manager ────────────────────────────────────────────────────────

def test_session_create_and_retrieve():
    s = USSDSession()
    s.new("sess1", "+250788000001")
    s.set_topic("sess1", "Umuhanda")
    assert s.get_topic("sess1") == "Umuhanda"


def test_session_pages():
    s = USSDSession()
    s.new("sess2", "+250788000002")
    s.set_pages("sess2", ["page one", "page two"])
    assert s.get_pages("sess2") == ["page one", "page two"]
    assert s.get_page_idx("sess2") == 0
    s.set_page_idx("sess2", 1)
    assert s.get_page_idx("sess2") == 1


def test_session_clear():
    s = USSDSession()
    s.new("sess3", "+250788000003")
    s.clear("sess3")
    assert s.get_topic("sess3") is None


# ── USSD flow integration ─────────────────────────────────────────────────

def test_first_contact_shows_home_menu(client):
    resp = client.post("/ussd", data={
        "sessionId": "AT_test_001",
        "serviceCode": "*290#",
        "phoneNumber": "+250788111111",
        "networkCode": "63902",
        "text": ""
    })
    body = resp.data.decode("utf-8")
    assert resp.status_code == 200
    assert body.startswith("CON ")
    assert "KINYARWANDA AI" in body


def test_exit_terminates_session(client):
    resp = client.post("/ussd", data={
        "sessionId": "AT_test_002",
        "serviceCode": "*290#",
        "phoneNumber": "+250788111112",
        "networkCode": "63902",
        "text": "0"
    })
    body = resp.data.decode("utf-8")
    assert body.startswith("END ")
    assert "Murakoze" in body


def test_government_services_menu(client):
    resp = client.post("/ussd", data={
        "sessionId": "AT_test_003",
        "serviceCode": "*290#",
        "phoneNumber": "+250788111113",
        "networkCode": "63902",
        "text": "2"
    })
    body = resp.data.decode("utf-8")
    assert "CON " in body
    assert "leta" in body.lower() or "LETA" in body


def test_health_menu(client):
    resp = client.post("/ussd", data={
        "sessionId": "AT_test_004",
        "serviceCode": "*290#",
        "phoneNumber": "+250788111114",
        "networkCode": "63902",
        "text": "3"
    })
    body = resp.data.decode("utf-8")
    assert "CON " in body
    assert "UBUZIMA" in body or "Ubuzima" in body


def test_health_endpoint(client):
    resp = client.get("/health")
    assert resp.status_code == 200
    data = resp.get_json()
    assert data["status"] == "ok"


# ── Knowledge base ────────────────────────────────────────────────────────

def test_kb_lookup_mobile_money():
    from ai_bridge import KinyarwandaAI
    ai = KinyarwandaAI()
    answer = ai._kb_lookup("mobile money MTN kohereza amafaranga")
    assert answer is not None
    assert len(answer) > 10


def test_kb_lookup_no_match_returns_none():
    from ai_bridge import KinyarwandaAI
    ai = KinyarwandaAI()
    result = ai._kb_lookup("xyzzy foobar quxquux")
    assert result is None


def test_kb_summary_style():
    from ai_bridge import KinyarwandaAI
    ai = KinyarwandaAI()
    answer = ai._kb_lookup("indangamuntu ID ikarita")
    assert answer is not None
    summary = ai._apply_style(answer, "summary")
    assert len(summary) < len(answer) or "." in summary


def test_offline_fallback_contains_contact():
    from ai_bridge import KinyarwandaAI
    ai = KinyarwandaAI()
    result = ai._offline_fallback("anything")
    assert "3883" in result or "114" in result or "RURA" in result
