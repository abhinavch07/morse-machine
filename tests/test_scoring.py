"""Tests for the LCWO style scoring."""

from trainer.scoring import score


def test_perfect_copy():
    result = score("SOS", "SOS")
    assert result.percent == 100
    assert result.distance == 0
    assert result.wrong == []


def test_one_swap():
    result = score("KMU", "KMX")
    assert result.distance == 1
    assert round(result.percent) == 67  # (3 - 1) / 3
    assert result.errors == {"U": 1}
    assert ("U", "X") in result.wrong


def test_one_missed_character():
    result = score("KMU", "KM")
    assert result.distance == 1
    assert result.errors == {"U": 1}


def test_one_extra_character():
    result = score("KM", "KMU")
    assert result.distance == 1
    # An extra typed character is not blamed on any sent character.
    assert result.errors == {}


def test_grouping_spaces_are_ignored():
    assert score("KM UR", "KMUR").percent == 100


def test_lowercase_is_accepted():
    assert score("KM", "km").percent == 100


def test_missed_marker_dash_counts_as_wrong():
    result = score("KMU", "KM-")
    assert result.distance == 1
    assert result.errors == {"U": 1}


def test_empty_scores_full():
    assert score("", "").percent == 100
