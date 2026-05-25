"""
USSD Menu strings — all text is in Kinyarwanda.

USSD protocol limits:
  - 182 characters per message (AT gateway sometimes allows 160 for safety)
  - Session responses: prefix "CON " (continues) or "END " (terminates)
  - No images, no formatting beyond newlines
"""

MENU_HOME = (
    "KINYARWANDA AI\n"
    "Murakaza neza!\n"
    "1. Baza ikibazo\n"
    "2. Serivisi za leta\n"
    "3. Ubuzima\n"
    "4. Uburezi\n"
    "5. Ikoranabuhanga\n"
    "0. Sohoka"
)

MENU_HELP = (
    "UBUFASHA\n"
    "Andika nimero ukunde,\n"
    "hanyuma ugaragaze.\n"
    "Igisubizo cyazagutangwa\n"
    "mu Kinyarwanda.\n"
    "0. Rudi"
)

MENU_ERROR = (
    "Habari ikibazo.\n"
    "Gerageza nyuma gato.\n"
    "0. Rudi ku menu"
)


def build_response(text: str, end: bool = False) -> str:
    """
    Prepend the AT-gateway session marker:
      CON = session continues (user can type more)
      END = session terminates (last message, phone hangs up)
    """
    prefix = "END " if end else "CON "
    return prefix + truncate_ussd(text)


def truncate_ussd(text: str, limit: int = 178) -> str:
    """
    Hard-truncate to the USSD limit.  We use 178 to leave room for the
    4-character prefix (CON  / END ).  If we must cut, append "…" so the
    user knows the message was trimmed.
    """
    if len(text) <= limit:
        return text
    return text[:limit - 1] + "…"
