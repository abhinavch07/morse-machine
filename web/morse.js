// Morse logic for the browser page: the code table, a decoder that turns key
// timings into letters, and the playout timing for received key events.
// No page, sound or network code here, so the Node tests can check it:
//   node --test tests/morse_js.test.mjs

// Decoder limits in ms, the same as the ESP32 decoder in firmware/src/main.cpp.
export const DIT_DAH_SPLIT_MS = 160;  // a press this long or longer is a dah
export const LETTER_GAP_MS = 250;     // a gap this long or longer ends the letter
export const WORD_GAP_MS = 2500;      // a gap this long or longer ends the word

// Received key events play this long after they were sent, so small network
// delays do not make the rhythm uneven.
export const PLAYOUT_DELAY_MS = 200;

// An event planned to play more than this far beyond the normal delay means
// the sender's clock jumped, for example the sender reloaded the page.
export const MAX_EARLY_MS = 1000;

// A received key held down longer than this is treated as stuck.
export const STUCK_KEY_MS = 2000;

// The code table, in the same order as CHAR_TO_MORSE in morse_core/codes.py.
// It must stay plain JSON between the two marker lines (no comments, no
// trailing comma), because tests/test_web_morse_table.py reads it from here
// and checks it matches the Python table exactly.
// The order matters: "=" comes before <BT>, so their shared code -...-
// decodes to "=", just like Python and the ESP32.
// MORSE TABLE START
export const CHAR_TO_MORSE = {
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
  ".": ".-.-.-",
  ",": "--..--",
  ";": "-.-.-.",
  "=": "-...-",
  "-": "-....-",
  "?": "..--..",
  "/": "-..-.",
  "<AR>": ".-.-.",
  "<SK>": "...-.-",
  "<BT>": "-...-",
  "<KN>": "-.--."
};
// MORSE TABLE END

// Code to text, keeping the first text seen for each code, like Python.
const MORSE_TO_CHAR = new Map();
for (const [text, code] of Object.entries(CHAR_TO_MORSE)) {
  if (!MORSE_TO_CHAR.has(code)) MORSE_TO_CHAR.set(code, text);
}

// Text for one code like "-.-", or null if the code is not in the table.
export function morseDecode(code) {
  return MORSE_TO_CHAR.get(code) ?? null;
}

// Turns key timings into letters. The same rules as KeyDecoder on the ESP32.
//   keyDown(ms): the key was just released after being down for ms.
//   keyUp(ms):   the key has now been up for ms. Call it again and again
//                while the key stays up, so a letter finishes as soon as the
//                gap is long enough, without waiting for the next press.
export class KeyDecoder {
  static MAX_SYMBOLS = 9;  // longer than any code in the table

  constructor(ditDahSplitMs = DIT_DAH_SPLIT_MS, letterGapMs = LETTER_GAP_MS,
              wordGapMs = WORD_GAP_MS) {
    this.ditDahSplitMs = ditDahSplitMs;
    this.letterGapMs = letterGapMs;
    this.wordGapMs = wordGapMs;
    this.code = "";            // dits and dahs of the letter so far
    this.symbolCount = 0;      // presses in this letter, can pass MAX_SYMBOLS
    this.lettersInWord = false;  // a letter came since the last word gap
  }

  keyDown(ms) {
    if (this.symbolCount < KeyDecoder.MAX_SYMBOLS) {
      this.code += ms < this.ditDahSplitMs ? "." : "-";
    }
    this.symbolCount++;
  }

  // Returns {letterDone, wordDone, letter}. Each is reported once per gap.
  // letter is the text, like "K" or "<AR>", or "*" for an unknown code.
  keyUp(ms) {
    const result = { letterDone: false, wordDone: false, letter: null };

    if (this.symbolCount > 0 && ms >= this.letterGapMs) {
      const text = this.symbolCount > KeyDecoder.MAX_SYMBOLS ? null : morseDecode(this.code);
      result.letterDone = true;
      result.letter = text ?? "*";
      this.code = "";
      this.symbolCount = 0;
      this.lettersInWord = true;
    }

    if (this.lettersInWord && ms >= this.wordGapMs) {
      result.wordDone = true;
      this.lettersInWord = false;
    }

    return result;
  }

  // The dits and dahs of the letter being keyed now, or "" between letters.
  currentCode() {
    return this.code;
  }
}

// Works out when to play each received event, on our own clock.
//
// Every sender has its own clock, so their t numbers mean nothing to us on
// their own. But the difference between two of their t values is a true
// time gap. So for each sender we remember one offset, set by their first
// event: play time = their t + offset, where the offset makes that first
// event play PLAYOUT_DELAY_MS from now. Later events keep the same offset,
// so the gaps between them are played exactly as they were sent.
export class Playout {
  constructor(delayMs = PLAYOUT_DELAY_MS, maxEarlyMs = MAX_EARLY_MS) {
    this.delayMs = delayMs;
    this.maxEarlyMs = maxEarlyMs;
    this.offsets = new Map();  // sender name to offset in ms
  }

  // senderT: the event's t from the sender. nowMs: our clock now.
  // Returns {playAt, reset}: when to play it on our clock, and whether the
  // sender's offset had to be set again.
  schedule(sender, senderT, nowMs) {
    let offset = this.offsets.get(sender);
    const playAt = offset === undefined ? null : senderT + offset;

    // Reset when it is the sender's first event, when this one came too late
    // for its slot, or when it is planned impossibly far ahead.
    const reset = playAt === null || playAt < nowMs ||
                  playAt > nowMs + this.delayMs + this.maxEarlyMs;
    if (reset) {
      offset = nowMs + this.delayMs - senderT;
      this.offsets.set(sender, offset);
    }
    return { playAt: senderT + offset, reset };
  }
}
