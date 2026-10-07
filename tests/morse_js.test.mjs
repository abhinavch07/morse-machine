// Unit tests for web/morse.js. They use Node's built-in test runner, so
// nothing needs installing. Run from the project folder with:
//   node --test tests/morse_js.test.mjs
// The decoder tests use the same timings as the ESP32 decoder tests: 15 WPM
// presses (dit 80 ms, dah 240 ms, gap inside a letter 80 ms), a 450 ms
// letter gap and a 3000 ms word gap.

import assert from "node:assert/strict";
import { test } from "node:test";

import {
  KeyDecoder,
  LETTER_GAP_MS,
  PLAYOUT_DELAY_MS,
  Playout,
  WORD_GAP_MS,
  morseDecode,
} from "../web/morse.js";

// Feed timings that take turns: press, gap, press, gap, and so on, and
// collect the text the way the page shows it.
function decode(ms) {
  const d = new KeyDecoder();
  let out = "";
  ms.forEach((value, i) => {
    if (i % 2 === 0) {
      d.keyDown(value);
    } else {
      const r = d.keyUp(value);
      if (r.letterDone) out += r.letter;
      if (r.wordDone) out += " ";
    }
  });
  return out;
}

test("morseDecode finds letters, prosigns and unknown codes", () => {
  assert.equal(morseDecode("-.-"), "K");
  assert.equal(morseDecode(".-.-."), "<AR>");
  assert.equal(morseDecode("-.--."), "<KN>");
  assert.equal(morseDecode("-...-"), "=");  // shared with <BT>, "=" wins, like Python
  assert.equal(morseDecode("........"), null);
});

test("decodes K", () => {
  assert.equal(decode([240, 80, 80, 80, 240, 3000]), "K ");
});

test("decodes VU with a letter gap and a word gap", () => {
  assert.equal(decode([80, 80, 80, 80, 80, 80, 240, 450, 80, 80, 80, 80, 240, 3000]), "VU ");
});

test("decodes <AR> as one prosign", () => {
  assert.equal(decode([80, 80, 240, 80, 80, 80, 240, 80, 80, 3000]), "<AR> ");
});

test("limits are exact, like the ESP32", () => {
  // 159 ms is a dit, 160 ms is a dah.
  assert.equal(decode([159, 450]), "E");
  assert.equal(decode([160, 450]), "T");

  const d = new KeyDecoder();
  d.keyDown(80);
  assert.equal(d.keyUp(LETTER_GAP_MS - 1).letterDone, false);
  assert.equal(d.keyUp(LETTER_GAP_MS).letterDone, true);
  assert.equal(d.keyUp(WORD_GAP_MS - 1).wordDone, false);
  assert.equal(d.keyUp(WORD_GAP_MS).wordDone, true);
});

test("a letter is reported once while the key stays up", () => {
  const d = new KeyDecoder();
  d.keyDown(240);
  assert.equal(d.keyUp(100).letterDone, false);
  assert.equal(d.keyUp(300).letter, "T");
  assert.equal(d.keyUp(400).letterDone, false);  // not again
});

test("no output before the first press", () => {
  const d = new KeyDecoder();
  const r = d.keyUp(10000);
  assert.equal(r.letterDone, false);
  assert.equal(r.wordDone, false);
});

test("unknown codes and too many presses give *", () => {
  assert.equal(decode([80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 450]), "*");
  const twelveDits = [];
  for (let i = 0; i < 12; i++) twelveDits.push(80, 80);
  twelveDits[twelveDits.length - 1] = 450;
  assert.equal(decode(twelveDits), "*");
});

test("currentCode grows while keying and clears after the letter", () => {
  const d = new KeyDecoder();
  d.keyDown(80);
  d.keyUp(80);
  d.keyDown(240);
  assert.equal(d.currentCode(), ".-");
  d.keyUp(450);
  assert.equal(d.currentCode(), "");
});

test("playout: the first event plays after the delay", () => {
  const p = new Playout();
  assert.deepEqual(p.schedule("esp32", 50000, 1000), { playAt: 1000 + PLAYOUT_DELAY_MS, reset: true });
});

test("playout: later events keep the sender's own gaps", () => {
  const p = new Playout();
  p.schedule("esp32", 50000, 1000);  // plays at 1200
  // Sent 240 ms later, arrived 300 ms later (the network was slow), still
  // plays exactly 240 ms after the first.
  assert.deepEqual(p.schedule("esp32", 50240, 1300), { playAt: 1440, reset: false });
});

test("playout: an event too late for its slot resets the delay", () => {
  const p = new Playout();
  p.schedule("esp32", 50000, 1000);  // plays at 1200
  // Its slot would be 1280, but it only arrived at 1500.
  assert.deepEqual(p.schedule("esp32", 50080, 1500), { playAt: 1700, reset: true });
  // The next one uses the new offset.
  assert.deepEqual(p.schedule("esp32", 50160, 1580), { playAt: 1780, reset: false });
});

test("playout: a jump far ahead in the sender's clock resets the delay", () => {
  const p = new Playout();
  p.schedule("phone", 1000, 1000);
  // The phone reloaded the page, and its clock now reads much higher.
  assert.deepEqual(p.schedule("phone", 900000, 2000), { playAt: 2200, reset: true });
});

test("playout: each sender has its own offset", () => {
  const p = new Playout();
  p.schedule("esp32", 50000, 1000);
  p.schedule("phone", 7, 1010);
  assert.equal(p.schedule("esp32", 50100, 1100).playAt, 1300);
  assert.equal(p.schedule("phone", 107, 1110).playAt, 1310);
});
