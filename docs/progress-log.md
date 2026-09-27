# Progress Log

## 2026-09-27 Phase 1, session 1: project setup

Built:
- Checked tools: Python 3.13.15 and git 2.51.0 are installed.
- Created the project virtual environment in .venv using python3.13.
- Created the repo folder layout from CLAUDE.md.
- Added docs files, a dev requirements file, a .gitignore, a README and an MIT licence.
- Installed pytest inside the venv.

Learned:
- A virtual environment keeps this project's Python packages separate from the system Python.
- Git does not track empty folders, so some phase folders appear later when they hold files.

Next:
- Start Phase 1 core logic: text to Morse encoding with tests.

## 2026-09-27 Phase 1, session 1 part 2: Morse encode and decode

Built:
- morse_core/codes.py: the ITU code table for letters, figures, the ASOC
  punctuation and the prosigns AR, SK, BT, KN. The break sign "=" and <BT>
  share the code -...-.
- morse_core/translate.py: encode(text) and decode(morse). Encode accepts
  lowercase and messy spacing. Both raise a clear ValueError on unknown input.
- tests/test_translate.py: 25 checks including SOS, PARIS, full round trips of
  letters and figures, each punctuation mark and prosign, lowercase, extra
  spaces and error cases. All pass.

Learned:
- Some Morse codes are shared. The break sign "=" and the prosign <BT> are the
  same code, so decoding cannot tell them apart. We chose to decode -...- to
  the printable "=" and documented that rule.
- Prosigns are sent as one run of dits and dahs, so we write them in text
  inside angle brackets like <AR> and parse them as one token.

Next:
- Add the timing engine (dit length from WPM, Farnsworth spacing).

