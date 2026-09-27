"""Tests for saving and loading trainer progress."""

from trainer.progress import (
    load_progress,
    save_result,
    passed,
    weakest_characters,
)


def test_pass_rule():
    assert passed(90)
    assert passed(95)
    assert not passed(89.9)


def test_default_progress_when_file_missing(tmp_path):
    progress = load_progress(str(tmp_path / "progress.json"))
    assert progress["current_lesson"] == 1
    assert progress["history"] == []


def test_save_then_load_and_advance_on_pass(tmp_path):
    path = str(tmp_path / "progress.json")
    save_result(path, lesson=3, char_wpm=20, effective_wpm=8,
                percent=95, errors={"U": 1}, date="2026-09-27")

    progress = load_progress(path)
    assert len(progress["history"]) == 1
    assert progress["history"][0]["lesson"] == 3
    assert progress["history"][0]["percent"] == 95
    # Passing at or above the current lesson moves the start lesson up.
    assert progress["current_lesson"] == 4


def test_no_advance_when_failing(tmp_path):
    path = str(tmp_path / "progress.json")
    save_result(path, lesson=1, char_wpm=20, effective_wpm=8,
                percent=50, errors={"M": 2})
    assert load_progress(path)["current_lesson"] == 1


def test_weakest_characters_added_up(tmp_path):
    path = str(tmp_path / "progress.json")
    save_result(path, 1, 20, 8, 50, {"M": 3, "K": 1})
    save_result(path, 1, 20, 8, 60, {"M": 1})

    weak = weakest_characters(load_progress(path), top=2)
    assert weak[0] == ("M", 4)
    assert ("K", 1) in weak
