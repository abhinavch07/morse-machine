"""Score a copied Morse practice round the way LCWO does.

We compare what was sent with what you typed using the Levenshtein edit
distance. That is the smallest number of single character changes (swap, miss
or extra) needed to turn one string into the other. The percentage is the share
of characters you got right. Grouping spaces are ignored, and case does not
matter. There is no sound here.
"""

from collections import namedtuple

# The result of scoring one round.
#   percent  : 0 to 100, the LCWO style accuracy
#   distance : the Levenshtein edit distance
#   length   : number of characters that were sent (spaces removed)
#   errors   : dict of sent character -> how many times it was missed or swapped
#   wrong    : list of (sent, typed) pairs that did not match, for display
ScoreResult = namedtuple("ScoreResult", "percent distance length errors wrong")


def _edit_ops(sent, typed):
    """Return the list of edit operations that turn sent into typed.

    Each item is a tuple (kind, sent_char, typed_char) where kind is one of
    "match", "sub" (swap), "del" (a sent character was missed) or "ins" (an
    extra character was typed). This is a standard edit distance table with a
    walk back through it to recover the steps.
    """
    rows, cols = len(sent), len(typed)
    table = [[0] * (cols + 1) for _ in range(rows + 1)]
    for i in range(rows + 1):
        table[i][0] = i
    for j in range(cols + 1):
        table[0][j] = j
    for i in range(1, rows + 1):
        for j in range(1, cols + 1):
            cost = 0 if sent[i - 1] == typed[j - 1] else 1
            table[i][j] = min(
                table[i - 1][j] + 1,       # miss a sent character
                table[i][j - 1] + 1,       # extra typed character
                table[i - 1][j - 1] + cost,  # match or swap
            )

    ops = []
    i, j = rows, cols
    while i > 0 or j > 0:
        cost = 0 if (i > 0 and j > 0 and sent[i - 1] == typed[j - 1]) else 1
        if i > 0 and j > 0 and table[i][j] == table[i - 1][j - 1] + cost:
            kind = "match" if cost == 0 else "sub"
            ops.append((kind, sent[i - 1], typed[j - 1]))
            i -= 1
            j -= 1
        elif i > 0 and table[i][j] == table[i - 1][j] + 1:
            ops.append(("del", sent[i - 1], None))
            i -= 1
        else:
            ops.append(("ins", None, typed[j - 1]))
            j -= 1
    ops.reverse()
    return ops


def score(sent, typed):
    """Score a copied round and return a ScoreResult.

    Spaces are stripped so grouping does not count, and both sides are made
    uppercase. A missed character can be typed as "-" and will simply not
    match. An empty sent string scores 100 percent.
    """
    clean_sent = sent.replace(" ", "").upper()
    clean_typed = typed.replace(" ", "").upper()

    ops = _edit_ops(clean_sent, clean_typed)
    distance = sum(1 for kind, _, _ in ops if kind != "match")
    length = len(clean_sent)
    percent = 100.0 if length == 0 else max(0.0, (length - distance) / length * 100)

    errors = {}
    for kind, sent_char, _ in ops:
        # A swap or a miss is the fault of a sent character we did not copy.
        if kind in ("sub", "del") and sent_char is not None:
            errors[sent_char] = errors.get(sent_char, 0) + 1

    wrong = [(s, t) for kind, s, t in ops if kind != "match"]
    return ScoreResult(percent=percent, distance=distance, length=length,
                       errors=errors, wrong=wrong)
