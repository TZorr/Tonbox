# Third-party notices

Tonbox is © 2026 T'Zorr and is distributed under the GNU Affero General
Public License, version 3 (AGPLv3) - see [LICENSE](LICENSE). This follows
from JUCE, below.

## JUCE

- **What:** the JUCE 9 framework (audio plugin client, audio and DSP
  modules, GUI). Not included in this repository - `CMakeLists.txt` expects
  its source at `~/JUCE` (or a path passed with `-DJUCE_PATH`) and builds it
  along with the plugin.
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
envelopes, mix, distortion, EQ, level and pan) is Tonbox's own code. The
send effects (`Source/Engine/Fx`) come from FMBox and are shared with its
siblings Kitbox and Rackbox; parts of the panel - the pads, the display, the
type buttons, the knob and the palette - come from FMBox, Kitbox and Rackbox
Plus. All of them are by the same author and under the same licence.

The overdrive's and fuzz's antiderivative anti-aliasing and the frequency
shifter follow ideas published in Signalsmith Audio's open-source DSP, but
were written independently: no code or tuned constants were copied. The
shifter's Hilbert allpass was designed for Rackbox by its
[`Scripts/HilbertDesign.cpp`](https://github.com/TZorr/Rackbox/blob/main/Scripts/HilbertDesign.cpp).
Two effects implement published algorithms: the plate is Jon Dattorro's
reverb from "Effect Design, Part 1" (J. Audio Eng. Soc., 1997), with the
delay lengths and output taps the paper gives, and the anti-aliasing follows
Parker, Zavalishin and Le Bivic (DAFx 2016).

No other library is linked, and nothing is downloaded at build or run time.
