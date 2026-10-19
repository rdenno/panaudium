# Task: custom JUCE GUI for the Panaudium synth plugin

Panaudium is an open-source (AGPL-3.0) sample-free synthesizer plugin (VST3 + Standalone) built with
JUCE 8.0.8, C++17, MSVC 2022. Today it shows JUCE's GenericAudioProcessorEditor (a long parameter list)
under a preset bar. Replace it with a proper hand-built interface inspired by Spectrasonics Omnisphere:
dark, polished, knob-based, with an FX rack page modelled on Omnisphere's FX rack (slots as horizontal
strips: big unit-name button on the left, labelled knobs, a model dropdown, a coloured themed panel per unit).

## Read first
- `src/plugin/PluginProcessor.h/.cpp` — the processor: `apvts` (AudioProcessorValueTreeState) holds every
  parameter; programs (factory presets) via getNumPrograms/getProgramName/setCurrentProgram/getCurrentProgram.
- `src/dsp/Params.h` — all parameter IDs and ranges (`buildParamDefs()`), enums (LayerParam, GlobalParam,
  Engine, FilterType, FxRackId, FxSlotParam), `engineNames()`, `filterNames()`, `engineLabels(engine)`
  (what Timbre/Shape/Character/FM Depth/FM Ratio mean for the current engine — use these as knob labels and
  update them live when the engine changes), `fxTypeNames()`, `kRackSlots`, `fxSlot()`.
  Layer parameter IDs are `A_<id>` / `B_<id>`, e.g. `A_cutoff`; global `g_gain`, `g_glide`;
  FX slot IDs are `fx<A|B|C><n>_<type|on|mix|model|k1..k6>`, e.g. `fxC4_k2`.
- `src/dsp/fx/FxUnit.h` — `fxInfo(type)` / `fxKnob(type, i)`: per FX unit its name, model list and up to 6
  knobs (name or nullptr if unused, real-world range, unit string). Slot knob parameters are normalised 0..1;
  display real values with `fxKnob(type,i).value(norm)` and the unit string.
- `src/dsp/Presets.h` — `factoryPresets()` gives name + category of each program (for a category browser).

## Files
Write the UI in `src/plugin/PluginEditor.h` and `src/plugin/PluginEditor.cpp` (CMake already compiles
PluginEditor.cpp). You may add more headers under `src/plugin/ui/` (header-only, included from
PluginEditor.cpp). Do not modify anything outside `src/plugin/` and do not modify PluginProcessor.*.
Keep the class name `PanaudiumEditor` and constructor `explicit PanaudiumEditor(PanaudiumProcessor&)`.
Public test hooks (required, used by an automated screenshot tool):
`void showPage(int page)` (0 = Layer A, 1 = Layer B, 2 = FX rack) and `void showFxRack(int rack)`
(0 = rack A, 1 = rack B, 2 = Common). They must update the UI synchronously (no async/timer dependence).

## Layout (default 1180 x 760, resizable with a fixed aspect ratio via setResizeLimits / a constrainer;
scale everything from a 1180x760 design using an AffineTransform or proportional layout)
1. Header bar: "PANAUDIUM" wordmark (custom-drawn, letter-spaced), preset browser = category ComboBox
   ("All" + categories) and preset ComboBox filtered by category, prev/next arrow buttons, master Gain knob
   (g_gain) and Glide knob (g_glide).
2. Page tabs: LAYER A | LAYER B | FX RACK (custom-drawn tab buttons; the active tab glows).
3. Layer page (same component for A and B, bound to the right prefix):
   - OSCILLATOR panel: layer On toggle, Engine ComboBox, three large knobs Timbre/Shape/Character with
     labels from engineLabels(engine), smaller Noise / FM Depth / FM Ratio knobs (label from engineLabels;
     dim/disable a knob whose label is "-").
   - PITCH: Octave, Semitone, Fine. UNISON: Voices, Detune, Spread, Drift.
   - FILTER: type ComboBox, Cutoff, Resonance, Key Track, Env Amount, Drive.
   - AMP ENVELOPE and FILTER ENVELOPE: A/D/S/R each, plus a small live drawing of the envelope shape.
   - LFO 1 (Rate, >Pitch, >Cutoff) and LFO 2 (Rate, >Timbre, >Pan).
   - OUTPUT: Level, Pan, Velocity Sens.
4. FX RACK page: sub-tabs A | B | COMMON (like Omnisphere's AUX/A/B/.../COMMON bar). Below, one horizontal
   strip per slot of the selected rack (3 or 4 strips stacked, each ~110 px tall at design size):
   - Left: a rounded "unit button" showing the unit name (e.g. STOMPBOX) — clicking opens a PopupMenu of all
     unit types (fxTypeNames) to choose the slot's type; a power LED toggle (slot On); a small horizontal
     Mix slider under the button.
   - Middle: the unit's knobs (only the knobs whose FxKnob name is non-null), each with its name and a value
     readout in real units (e.g. "350 ms", "2.4 kHz", "-6.0 dB", "45 %"), plus a Model ComboBox listing
     fxInfo(type).models (only the valid entries; model parameter is an int 0..7).
   - Right: a decorative themed area with the unit name in a stylised font.
   - Each unit type gets its own colour theme for the strip background/accent (e.g. Stompbox burnt orange,
     Phaser dark crimson, Chorus teal-blue, Flanger purple, Tremolo amber, Rotary walnut brown, Delay steel
     blue, Pro-Verb deep red, Innerspace dark teal-green, Shimmer icy cyan, EQ graphite, Compressor slate,
     Lo-Fi toxic green, Ring/Shift magenta, Wah gold, Tape cream/brown, Amp black-and-gold). OFF slots are
     dimmed and show "EMPTY SLOT".
   - Up/down arrow buttons at the right edge move the slot within its rack: swap all 10 parameters of the two
     slots (type, on, mix, model, k1..k6) via the parameters' setValueNotifyingHost (wrap in
     beginChangeGesture/endChangeGesture).
   - When a slot's type changes (from the menu, a preset, or host automation) the strip must update its
     labels, visible knobs, model list and colours. Listen to the type parameter (AudioProcessorValueTreeState::
     Listener or a 30 Hz Timer that polls) and update on the message thread only.
   - When the user picks a new unit type from the menu, also reset that slot's knobs to the unit's defaults
     (fxKnob(type,i).defNorm()), model to 0, mix to fxInfo(type).defaultMix, and On to 1.
5. Footer: one line of small text: currently selected preset category/name, and "sample-free synthesis".

## Look and feel
A custom `juce::LookAndFeel_V4` subclass: dark charcoal/blue-grey background with subtle gradients and
panel bevels, rotary knobs drawn as a dark knob body with a coloured value arc and pointer (accent colour per
panel), custom ComboBox and button drawing, readable font sizes (min 11 px at design size). Use only JUCE
drawing primitives (no image assets). Bind every control with
AudioProcessorValueTreeState::SliderAttachment / ComboBoxAttachment / ButtonAttachment (keep attachments
alive; recreate an FX strip's ComboBox items BEFORE (re)attaching when the model list changes). Value text
for layer knobs: use the parameter's own text (getText) or format sensibly (Hz/kHz, ms/s, %, dB, semitones).
Preset ComboBox: selecting calls processor.setCurrentProgram(index); keep it in sync if the host changes
program (poll getCurrentProgram in a timer).

## Constraints
- JUCE 8 API (e.g. use `juce::FontOptions` for fonts; `juce::Font(float)` constructor is deprecated).
- No new dependencies, no binary assets, no web views.
- Must compile cleanly with MSVC (/W4 warnings tolerated but avoid errors). You cannot run any shell commands
  (they are denied and abort your run), so you cannot compile: write carefully, check every JUCE API you use
  exists in JUCE 8 (the JUCE sources are in `JUCE/modules/` in this workspace — read them to verify
  signatures), include what you use.
- Real-time safety is not a concern in the editor, but never touch the processor's DSP objects directly;
  go through apvts parameters only.

## Report back
Files created/changed, a short description of the component structure, and anything you are unsure compiles.
