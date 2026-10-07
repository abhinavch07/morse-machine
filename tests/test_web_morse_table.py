"""Checks that the Morse table in web/morse.js matches morse_core.

The browser cannot use the Python table, so web/morse.js has its own copy.
This test reads that copy and fails if the two ever drift apart.
"""

import json
import re
from pathlib import Path

from morse_core.codes import CHAR_TO_MORSE

MORSE_JS = Path(__file__).resolve().parent.parent / "web" / "morse.js"

# The table sits between two marker lines as plain JSON.
TABLE_PATTERN = re.compile(
    r"// MORSE TABLE START\s*export const CHAR_TO_MORSE = (\{.*?\});\s*// MORSE TABLE END",
    re.DOTALL,
)


def js_table():
    match = TABLE_PATTERN.search(MORSE_JS.read_text())
    assert match, "could not find the table between the markers in web/morse.js"
    return json.loads(match.group(1))  # keeps the order of the entries


def test_web_table_matches_morse_core():
    assert js_table() == CHAR_TO_MORSE


def test_web_table_has_the_same_order():
    # Order decides which text wins when two share a code: "=" before <BT>
    # makes -...- decode to "=" in both.
    assert list(js_table()) == list(CHAR_TO_MORSE)


def test_web_table_has_the_prosigns():
    table = js_table()
    for prosign in ["<AR>", "<SK>", "<BT>", "<KN>"]:
        assert prosign in table
