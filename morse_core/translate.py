"""Turn text into Morse and Morse back into text.

Rules for the Morse string:
- Dits are "." and dahs are "-".
- Inside a word, characters are separated by one space.
- Words are separated by " / " (slash with a space on each side).

There is no timing here. This is only the written form of Morse.
"""

from .codes import CHAR_TO_MORSE, MORSE_TO_CHAR


def _tokenize_word(word):
    """Break one word into a list of tokens.

    A token is either a prosign like "<AR>" or a single character. We scan left
    to right. When we meet "<" we read up to the matching ">" and treat the
    whole thing as one prosign token.
    """
    tokens = []
    i = 0
    while i < len(word):
        if word[i] == "<":
            end = word.find(">", i)
            if end == -1:
                raise ValueError(
                    f"Unclosed prosign in {word!r}: a '<' has no matching '>'"
                )
            tokens.append(word[i:end + 1])
            i = end + 1
        else:
            tokens.append(word[i])
            i += 1
    return tokens


def encode(text):
    """Convert text to a Morse string.

    Lowercase is accepted and treated the same as uppercase. Runs of spaces are
    collapsed and treated as one word gap. Any character or prosign not in the
    table raises a ValueError that names the offending token.
    """
    words = text.upper().split()
    encoded_words = []
    for word in words:
        codes = []
        for token in _tokenize_word(word):
            if token not in CHAR_TO_MORSE:
                raise ValueError(
                    f"Cannot encode {token!r}: it is not in the Morse table"
                )
            codes.append(CHAR_TO_MORSE[token])
        encoded_words.append(" ".join(codes))
    return " / ".join(encoded_words)


def decode(morse):
    """Convert a Morse string back to text.

    Words may be separated by "/" with any spacing around it. Extra spaces
    between codes are ignored. Any code that is not known raises a ValueError
    that names the offending code.
    """
    decoded_words = []
    for word in morse.strip().split("/"):
        codes = word.split()
        if not codes:
            continue
        chars = []
        for code in codes:
            if code not in MORSE_TO_CHAR:
                raise ValueError(
                    f"Cannot decode {code!r}: it is not a known Morse code"
                )
            chars.append(MORSE_TO_CHAR[code])
        decoded_words.append("".join(chars))
    return " ".join(decoded_words)
