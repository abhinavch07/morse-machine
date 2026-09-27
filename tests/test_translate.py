"""Tests for the Morse encode and decode functions.

These check known examples, full round trips, punctuation, prosigns, lowercase
input, forgiving spacing, and clear errors on unknown input.
"""

import string

import pytest

from morse_core.translate import encode, decode


def test_sos():
    assert encode("SOS") == "... --- ..."
    assert decode("... --- ...") == "SOS"


def test_paris():
    # PARIS is the standard word used for Morse speed.
    assert encode("PARIS") == ".--. .- .-. .. ..."
    assert decode(".--. .- .-. .. ...") == "PARIS"


def test_word_gap():
    assert encode("HI THERE") == ".... .. / - .... . .-. ."
    assert decode(".... .. / - .... . .-. .") == "HI THERE"


def test_all_letters_round_trip():
    letters = string.ascii_uppercase
    assert decode(encode(letters)) == letters


def test_all_figures_round_trip():
    figures = "0123456789"
    assert decode(encode(figures)) == figures


@pytest.mark.parametrize(
    "char, code",
    [
        (".", ".-.-.-"),   # full stop
        (",", "--..--"),   # comma
        (";", "-.-.-."),   # semicolon
        ("=", "-...-"),    # break sign
        ("-", "-....-"),   # hyphen
        ("?", "..--.."),   # question mark
    ],
)
def test_each_punctuation_mark(char, code):
    assert encode(char) == code
    assert decode(code) == char


@pytest.mark.parametrize(
    "prosign, code",
    [
        ("<AR>", ".-.-."),
        ("<SK>", "...-.-"),
        ("<BT>", "-...-"),
        ("<KN>", "-.--."),
    ],
)
def test_each_prosign_encodes(prosign, code):
    assert encode(prosign) == code


def test_prosigns_round_trip_except_shared_bt():
    # <AR>, <SK> and <KN> have unique codes, so they round trip cleanly.
    for prosign in ("<AR>", "<SK>", "<KN>"):
        assert decode(encode(prosign)) == prosign
    # <BT> shares its code with the break sign "=". By our rule the shared code
    # -...- decodes to the printable "=", not back to <BT>.
    assert encode("<BT>") == "-...-"
    assert decode("-...-") == "="


def test_prosign_inside_text():
    assert encode("GM<AR>") == "--. -- .-.-."
    assert decode("--. -- .-.-.") == "GM<AR>"


def test_lowercase_is_accepted():
    assert encode("sos") == encode("SOS")
    assert encode("paris") == encode("PARIS")
    assert encode("cq") == "-.-. --.-"


def test_extra_spaces_in_text_are_forgiven():
    assert encode("  HI   THERE  ") == ".... .. / - .... . .-. ."


def test_extra_spaces_in_morse_are_forgiven():
    assert decode("  ...   ---   ...  ") == "SOS"
    assert decode("....  ..  /  -  .... . .-. .") == "HI THERE"


def test_empty_input():
    assert encode("") == ""
    assert decode("") == ""


def test_unknown_character_raises():
    with pytest.raises(ValueError) as info:
        encode("HELLO@WORLD")
    assert "@" in str(info.value)


def test_unknown_prosign_raises():
    with pytest.raises(ValueError):
        encode("<ZZ>")


def test_unclosed_prosign_raises():
    with pytest.raises(ValueError):
        encode("A<AR")


def test_unknown_code_raises():
    with pytest.raises(ValueError) as info:
        decode("........")
    assert "........" in str(info.value)
