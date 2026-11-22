# Third-party notices

Tonbox is © 2026 T'Zorr and is distributed under the GNU Affero General
Public License, version 3 (AGPLv3) - see [LICENSE](LICENSE). This follows
from JUCE, below.

## JUCE

- **What:** the JUCE 9 framework (audio plugin client, GUI).
  Not included in this repository - `CMakeLists.txt` expects it at `~/JUCE`
  (or a path passed with `-DJUCE_PATH`) and links it as an external framework.
- **Copyright:** © Raw Material Software Limited - <https://juce.com>.
- **License:** JUCE 9 is dual-licensed. This project uses JUCE's free tier,
  the GNU Affero General Public License, version 3 (AGPLv3). Because JUCE is
  AGPLv3 here and Tonbox links it, Tonbox is itself AGPLv3 in turn - the
  AGPL's own requirement for any work that incorporates AGPL-licensed code.
  The full JUCE licence terms are at
  <https://github.com/juce-framework/JUCE/blob/master/LICENSE.md>.
- **What AGPLv3 asks of anyone who distributes this plugin (including a
  built AU/VST3, not only source):** make the complete corresponding source
  available under the same licence, and preserve the copyright and licence
  notices. This repository already is that source.

## Everything else

The synthesis engine (oscillator, pitch modulation, filtered noise and its
envelopes, mix, distortion, EQ, level and pan) and the panel are Tonbox's own
code. The send effects (`Source/Engine/Fx`) come from FMBox and are shared
with its siblings Kitbox and Rackbox. The frequency shifter follows ideas
published in Signalsmith Audio's open-source DSP, but was written
independently: no code or tuned constants were copied; its Hilbert allpass
was designed for Rackbox by its
[`Scripts/HilbertDesign.cpp`](https://github.com/TZorr/Rackbox/blob/main/Scripts/HilbertDesign.cpp).
No other library is linked, and
nothing is downloaded at build or run time.
