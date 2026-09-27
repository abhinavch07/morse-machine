"""ITU Morse code table.

This is the single source of truth for which text maps to which Morse code.
A code is written with "." for a dit and "-" for a dah. This file has no
timing and no audio. It only says what the codes are.
"""

# Text token to Morse code.
# A token is normally one character. Prosigns are multi letter symbols sent as
# one run, written here in angle brackets like <AR>.
#
# Note on a shared code: the break sign "=" and the prosign <BT> are the same
# code -...-. They both appear below on purpose. For decoding we prefer the
# printable "=" (see the reverse table further down).
CHAR_TO_MORSE = {
    # Letters A to Z
    "A": ".-",
    "B": "-...",
    "C": "-.-.",
    "D": "-..",
    "E": ".",
    "F": "..-.",
    "G": "--.",
    "H": "....",
    "I": "..",
    "J": ".---",
    "K": "-.-",
    "L": ".-..",
    "M": "--",
    "N": "-.",
    "O": "---",
    "P": ".--.",
    "Q": "--.-",
    "R": ".-.",
    "S": "...",
    "T": "-",
    "U": "..-",
    "V": "...-",
    "W": ".--",
    "X": "-..-",
    "Y": "-.--",
    "Z": "--..",
    # Figures 0 to 9
    "0": "-----",
    "1": ".----",
    "2": "..---",
    "3": "...--",
    "4": "....-",
    "5": ".....",
    "6": "-....",
    "7": "--...",
    "8": "---..",
    "9": "----.",
    # Punctuation from the ASOC test
    ".": ".-.-.-",   # full stop
    ",": "--..--",   # comma
    ";": "-.-.-.",   # semicolon
    "=": "-...-",    # break sign (same code as <BT>)
    "-": "-....-",   # hyphen
    "?": "..--..",   # question mark
    # Prosigns, written in text inside angle brackets
    "<AR>": ".-.-.",   # end of message
    "<SK>": "...-.-",  # end of contact
    "<BT>": "-...-",   # break or new paragraph (same code as "=")
    "<KN>": "-.--.",   # go ahead, named station only
}

# Morse code to text, used for decoding.
# We build it by walking the table once and keeping the first token seen for
# each code. Because "=" is listed before <BT> above, the shared code -...-
# decodes to the printable "=". This keeps decoded output as plain text where a
# printable form exists, and reserves angle bracket prosigns for codes that have
# no punctuation twin (<AR>, <SK>, <KN>).
MORSE_TO_CHAR = {}
for _char, _code in CHAR_TO_MORSE.items():
    if _code not in MORSE_TO_CHAR:
        MORSE_TO_CHAR[_code] = _char
