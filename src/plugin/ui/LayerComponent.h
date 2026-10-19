#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PanaudiumKnob.h"
#include "EnvelopeView.h"
#include "../../dsp/Params.h"
#include "../PluginProcessor.h"

class LayerComponent : public juce::Component {
public:
    LayerComponent(PanaudiumProcessor& processor, int layer)
        : proc(processor), layerIndex(layer),
          prefix(layer == 0 ? "A_" : "B_"),
          layerName(layer == 0 ? "LAYER A" : "LAYER B") {

        auto pctFmt = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
        auto timeFmt = [](double v) {
            if (v < 1.0) return juce::String(juce::roundToInt(v * 1000.0)) + " ms";
            return juce::String(v, 2) + " s";
        };

        // --- 1. OSCILLATOR ---
        onToggle.setButtonText("");
        addAndMakeVisible(onToggle);
        onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, prefix + "on", onToggle);

        const auto& engines = op::engineNames();
        for (int i = 0; i < (int)engines.size(); ++i) {
            engineCombo.addItem(engines[i], i + 1);
        }
        addAndMakeVisible(engineCombo);
        engineAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, prefix + "engine", engineCombo);
        engineCombo.onChange = [this] { updateEngineLabels(); };

        setupKnob(timbreKnob, prefix + "timbre", 0xffe5a83b, pctFmt, true);
        setupKnob(shapeKnob, prefix + "shape", 0xffe5a83b, pctFmt, true);
        setupKnob(charKnob, prefix + "char", 0xffe5a83b, pctFmt, true);
        setupKnob(noiseKnob, prefix + "noise", 0xffe5a83b, pctFmt, false);
        setupKnob(fmDepthKnob, prefix + "fmdepth", 0xffe5a83b, pctFmt, false);
        setupKnob(fmRatioKnob, prefix + "fmratio", 0xffe5a83b, [](double v) { return juce::String(v, 2) + " x"; }, false);

        // --- 2. PITCH & UNISON ---
        setupKnob(octKnob, prefix + "oct", 0xffa855f7, [](double v) {
            int i = juce::roundToInt(v); return (i > 0 ? "+" : "") + juce::String(i);
        });
        setupKnob(semiKnob, prefix + "semi", 0xffa855f7, [](double v) {
            int i = juce::roundToInt(v); return (i > 0 ? "+" : "") + juce::String(i) + " st";
        });
        setupKnob(fineKnob, prefix + "fine", 0xffa855f7, [](double v) {
            int i = juce::roundToInt(v); return (i > 0 ? "+" : "") + juce::String(i) + " ct";
        });

        setupKnob(uniVoicesKnob, prefix + "univoices", 0xff818cf8, [](double v) {
            return juce::String(juce::roundToInt(v));
        });
        setupKnob(uniDetuneKnob, prefix + "unidetune", 0xff818cf8, [](double v) {
            return juce::String(juce::roundToInt(v)) + " ct";
        });
        setupKnob(uniSpreadKnob, prefix + "unispread", 0xff818cf8, pctFmt);
        setupKnob(driftKnob, prefix + "drift", 0xff818cf8, pctFmt);

        // --- 3. FILTER ---
        const auto& filters = op::filterNames();
        for (int i = 0; i < (int)filters.size(); ++i) {
            ftypeCombo.addItem(filters[i], i + 1);
        }
        addAndMakeVisible(ftypeCombo);
        ftypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, prefix + "ftype", ftypeCombo);

        setupKnob(cutoffKnob, prefix + "cutoff", 0xff38bdf8, [](double v) {
            if (v >= 1000.0) return juce::String(v * 0.001, 1) + " kHz";
            return juce::String(juce::roundToInt(v)) + " Hz";
        }, true);
        setupKnob(resoKnob, prefix + "reso", 0xff38bdf8, pctFmt);
        setupKnob(driveKnob, prefix + "drive", 0xff38bdf8, pctFmt);
        setupKnob(keyTrackKnob, prefix + "keytrack", 0xff38bdf8, pctFmt);
        setupKnob(filtEnvKnob, prefix + "fenvamt", 0xff38bdf8, [](double v) {
            return (v > 0.0 ? "+" : "") + juce::String(v, 1) + " oct";
        });

        // --- 4. ENVELOPES ---
        addAndMakeVisible(ampEnvView);
        setupKnob(ampAKnob, prefix + "aa", 0xfffb923c, timeFmt);
        setupKnob(ampDKnob, prefix + "ad", 0xfffb923c, timeFmt);
        setupKnob(ampSKnob, prefix + "as", 0xfffb923c, pctFmt);
        setupKnob(ampRKnob, prefix + "ar", 0xfffb923c, timeFmt);

        auto updateAmp = [this] {
            ampEnvView.setADSR((float)ampAKnob.slider.getValue(), (float)ampDKnob.slider.getValue(),
                               (float)ampSKnob.slider.getValue(), (float)ampRKnob.slider.getValue());
        };
        ampAKnob.slider.onValueChange = updateAmp;
        ampDKnob.slider.onValueChange = updateAmp;
        ampSKnob.slider.onValueChange = updateAmp;
        ampRKnob.slider.onValueChange = updateAmp;

        addAndMakeVisible(filtEnvView);
        setupKnob(filtAKnob, prefix + "fa", 0xff06b6d4, timeFmt);
        setupKnob(filtDKnob, prefix + "fd", 0xff06b6d4, timeFmt);
        setupKnob(filtSKnob, prefix + "fs", 0xff06b6d4, pctFmt);
        setupKnob(filtRKnob, prefix + "fr", 0xff06b6d4, timeFmt);

        auto updateFilt = [this] {
            filtEnvView.setADSR((float)filtAKnob.slider.getValue(), (float)filtDKnob.slider.getValue(),
                                (float)filtSKnob.slider.getValue(), (float)filtRKnob.slider.getValue());
        };
        filtAKnob.slider.onValueChange = updateFilt;
        filtDKnob.slider.onValueChange = updateFilt;
        filtSKnob.slider.onValueChange = updateFilt;
        filtRKnob.slider.onValueChange = updateFilt;

        // --- 5. MODULATION ---
        auto hzFmt = [](double v) { return juce::String(v, v < 1.0 ? 2 : 1) + " Hz"; };
        setupKnob(l1RateKnob, prefix + "l1rate", 0xff10b981, hzFmt);
        setupKnob(l1PitchKnob, prefix + "l1pitch", 0xff10b981, [](double v) { return juce::String(v, 1) + " st"; });
        setupKnob(l1CutKnob, prefix + "l1cut", 0xff10b981, [](double v) { return juce::String(v, 1) + " oct"; });

        setupKnob(l2RateKnob, prefix + "l2rate", 0xff10b981, hzFmt);
        setupKnob(l2TimbreKnob, prefix + "l2timbre", 0xff10b981, pctFmt);
        setupKnob(l2PanKnob, prefix + "l2pan", 0xff10b981, pctFmt);

        // --- 6. OUTPUT ---
        setupKnob(levelKnob, prefix + "level", 0xffcbd5e1, pctFmt);
        setupKnob(panKnob, prefix + "pan", 0xffcbd5e1, [](double v) {
            int p = juce::roundToInt(v * 100.0);
            if (p == 0) return juce::String("C");
            if (p < 0) return juce::String(-p) + "% L";
            return juce::String(p) + "% R";
        });
        setupKnob(velSensKnob, prefix + "velsens", 0xffcbd5e1, pctFmt);

        updateEngineLabels();
        updateAmp();
        updateFilt();
    }

    void updateEngineLabels() {
        int eng = 0;
        if (auto* p = proc.apvts.getParameter(prefix + "engine"))
            eng = juce::roundToInt(p->convertFrom0to1(p->getValue()));
        if (eng < 0 || eng >= op::E_NumEngines) eng = 0;

        if (engineCombo.getSelectedItemIndex() != eng)
            engineCombo.setSelectedItemIndex(eng, juce::dontSendNotification);

        const auto& lbl = op::engineLabels(eng);

        timbreKnob.setLabel(lbl.timbre);
        timbreKnob.setDimmed(juce::String(lbl.timbre) == "-");

        shapeKnob.setLabel(lbl.shape);
        shapeKnob.setDimmed(juce::String(lbl.shape) == "-");

        charKnob.setLabel(lbl.chr);
        charKnob.setDimmed(juce::String(lbl.chr) == "-");

        noiseKnob.setLabel("Noise");

        fmDepthKnob.setLabel(lbl.fmDepth);
        fmDepthKnob.setDimmed(juce::String(lbl.fmDepth) == "-");

        fmRatioKnob.setLabel(lbl.fmRatio);
        fmRatioKnob.setDimmed(juce::String(lbl.fmRatio) == "-");
    }

    void updateEnvelopes() {
        ampEnvView.setADSR((float)ampAKnob.slider.getValue(), (float)ampDKnob.slider.getValue(),
                           (float)ampSKnob.slider.getValue(), (float)ampRKnob.slider.getValue());
        filtEnvView.setADSR((float)filtAKnob.slider.getValue(), (float)filtDKnob.slider.getValue(),
                            (float)filtSKnob.slider.getValue(), (float)filtRKnob.slider.getValue());
    }

    void paint(juce::Graphics& g) override {
        // Panels background & headers
        auto drawPanel = [&](juce::Rectangle<int> r, const juce::String& title, juce::Colour accent) {
            const auto rf = r.toFloat();
            // Background
            g.setColour(juce::Colour(0xff161820));
            g.fillRoundedRectangle(rf, 5.0f);
            g.setColour(juce::Colour(0xff282c38));
            g.drawRoundedRectangle(rf, 5.0f, 1.0f);

            // Title bar
            if (!title.isEmpty()) {
                g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
                g.setColour(accent);
                g.drawText(title, r.getX() + 10, r.getY() + 7, r.getWidth() - 20, 16, juce::Justification::left, true);
                // Subtle divider
                g.setColour(juce::Colour(0x18ffffff));
                g.drawHorizontalLine(r.getY() + 27, (float)r.getX() + 6.0f, (float)r.getRight() - 6.0f);
            }
        };

        // Top row panels
        drawPanel(oscBounds, "OSCILLATOR", juce::Colour(0xffe5a83b));
        drawPanel(filterBounds, "FILTER", juce::Colour(0xff38bdf8));
        drawPanel(pitchBounds, "PITCH", juce::Colour(0xffa855f7));
        drawPanel(unisonBounds, "UNISON", juce::Colour(0xff818cf8));

        // Bottom row panels
        drawPanel(ampBounds, "AMP ENVELOPE", juce::Colour(0xfffb923c));
        drawPanel(filtEnvBounds, "FILTER ENVELOPE", juce::Colour(0xff06b6d4));
        drawPanel(modBounds, "MODULATION", juce::Colour(0xff10b981));
        drawPanel(outputBounds, "OUTPUT", juce::Colour(0xffcbd5e1));

        // Subheaders inside Modulation
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.setColour(juce::Colour(0xff10b981).withAlpha(0.85f));
        g.drawText("LFO 1", modBounds.getX() + 12, modBounds.getY() + 32, 100, 16, juce::Justification::left);
        g.drawText("LFO 2", modBounds.getX() + 12, modBounds.getY() + 168, 100, 16, juce::Justification::left);
    }

    void resized() override {
        // Uniform grid layout (1180 x 636)
        oscBounds = juce::Rectangle<int>(10, 8, 560, 296);
        filterBounds = juce::Rectangle<int>(580, 8, 340, 296);
        pitchBounds = juce::Rectangle<int>(930, 8, 240, 142);
        unisonBounds = juce::Rectangle<int>(930, 156, 240, 148);

        ampBounds = juce::Rectangle<int>(10, 312, 275, 312);
        filtEnvBounds = juce::Rectangle<int>(295, 312, 275, 312);
        modBounds = juce::Rectangle<int>(580, 312, 340, 312);
        outputBounds = juce::Rectangle<int>(930, 312, 240, 312);

        // --- 1. OSCILLATOR ---
        onToggle.setBounds(oscBounds.getX() + 110, oscBounds.getY() + 3, 22, 22);
        engineCombo.setBounds(oscBounds.getX() + 140, oscBounds.getY() + 3, 210, 22);

        // Top 3 large knobs
        const int oscKnobY1 = oscBounds.getY() + 36;
        const int oscKnobW1 = 96;
        const int oscKnobH1 = 118;
        timbreKnob.setBounds(oscBounds.getX() + 40, oscKnobY1, oscKnobW1, oscKnobH1);
        shapeKnob.setBounds(oscBounds.getX() + 232, oscKnobY1, oscKnobW1, oscKnobH1);
        charKnob.setBounds(oscBounds.getX() + 424, oscKnobY1, oscKnobW1, oscKnobH1);

        // Bottom 3 smaller knobs
        const int oscKnobY2 = oscBounds.getY() + 166;
        const int oscKnobW2 = 82;
        const int oscKnobH2 = 104;
        noiseKnob.setBounds(oscBounds.getX() + 47, oscKnobY2, oscKnobW2, oscKnobH2);
        fmDepthKnob.setBounds(oscBounds.getX() + 239, oscKnobY2, oscKnobW2, oscKnobH2);
        fmRatioKnob.setBounds(oscBounds.getX() + 431, oscKnobY2, oscKnobW2, oscKnobH2);

        // --- 2. FILTER ---
        ftypeCombo.setBounds(filterBounds.getX() + 100, filterBounds.getY() + 3, 170, 22);

        // Cutoff large knob + Reso + Drive
        cutoffKnob.setBounds(filterBounds.getX() + 16, filterBounds.getY() + 36, 100, 118);
        resoKnob.setBounds(filterBounds.getX() + 130, filterBounds.getY() + 42, 86, 110);
        driveKnob.setBounds(filterBounds.getX() + 234, filterBounds.getY() + 42, 86, 110);

        keyTrackKnob.setBounds(filterBounds.getX() + 45, filterBounds.getY() + 166, 86, 104);
        filtEnvKnob.setBounds(filterBounds.getX() + 195, filterBounds.getY() + 166, 86, 104);

        // --- 3. PITCH ---
        const int pW = 68;
        octKnob.setBounds(pitchBounds.getX() + 12, pitchBounds.getY() + 34, pW, 94);
        semiKnob.setBounds(pitchBounds.getX() + 86, pitchBounds.getY() + 34, pW, 94);
        fineKnob.setBounds(pitchBounds.getX() + 160, pitchBounds.getY() + 34, pW, 94);

        // --- UNISON ---
        const int uW = 52;
        uniVoicesKnob.setBounds(unisonBounds.getX() + 10, unisonBounds.getY() + 34, uW, 96);
        uniDetuneKnob.setBounds(unisonBounds.getX() + 66, unisonBounds.getY() + 34, uW, 96);
        uniSpreadKnob.setBounds(unisonBounds.getX() + 122, unisonBounds.getY() + 34, uW, 96);
        driftKnob.setBounds(unisonBounds.getX() + 178, unisonBounds.getY() + 34, uW, 96);

        // --- 4. AMP ENVELOPE ---
        ampEnvView.setBounds(ampBounds.getX() + 12, ampBounds.getY() + 34, ampBounds.getWidth() - 24, 82);
        const int envKW = 58;
        ampAKnob.setBounds(ampBounds.getX() + 12, ampBounds.getY() + 130, envKW, 104);
        ampDKnob.setBounds(ampBounds.getX() + 76, ampBounds.getY() + 130, envKW, 104);
        ampSKnob.setBounds(ampBounds.getX() + 140, ampBounds.getY() + 130, envKW, 104);
        ampRKnob.setBounds(ampBounds.getX() + 204, ampBounds.getY() + 130, envKW, 104);

        // --- FILTER ENVELOPE ---
        filtEnvView.setBounds(filtEnvBounds.getX() + 12, filtEnvBounds.getY() + 34, filtEnvBounds.getWidth() - 24, 82);
        filtAKnob.setBounds(filtEnvBounds.getX() + 12, filtEnvBounds.getY() + 130, envKW, 104);
        filtDKnob.setBounds(filtEnvBounds.getX() + 76, filtEnvBounds.getY() + 130, envKW, 104);
        filtSKnob.setBounds(filtEnvBounds.getX() + 140, filtEnvBounds.getY() + 130, envKW, 104);
        filtRKnob.setBounds(filtEnvBounds.getX() + 204, filtEnvBounds.getY() + 130, envKW, 104);

        // --- 5. MODULATION ---
        const int modKW = 90;
        l1RateKnob.setBounds(modBounds.getX() + 20, modBounds.getY() + 52, modKW, 104);
        l1PitchKnob.setBounds(modBounds.getX() + 125, modBounds.getY() + 52, modKW, 104);
        l1CutKnob.setBounds(modBounds.getX() + 230, modBounds.getY() + 52, modKW, 104);

        l2RateKnob.setBounds(modBounds.getX() + 20, modBounds.getY() + 188, modKW, 104);
        l2TimbreKnob.setBounds(modBounds.getX() + 125, modBounds.getY() + 188, modKW, 104);
        l2PanKnob.setBounds(modBounds.getX() + 230, modBounds.getY() + 188, modKW, 104);

        // --- 6. OUTPUT ---
        const int outKW = 68;
        levelKnob.setBounds(outputBounds.getX() + 12, outputBounds.getY() + 48, outKW, 104);
        panKnob.setBounds(outputBounds.getX() + 86, outputBounds.getY() + 48, outKW, 104);
        velSensKnob.setBounds(outputBounds.getX() + 160, outputBounds.getY() + 48, outKW, 104);
    }

private:
    void setupKnob(PanaudiumKnob& knob, const juce::String& paramId, juce::uint32 colorRgb,
                   std::function<juce::String(double)> fmt, bool large = false) {
        addAndMakeVisible(knob);
        knob.large = large;
        knob.attach(proc.apvts, paramId);
        knob.setAccentColour(juce::Colour(colorRgb));
        if (fmt) knob.setFormatFunction(fmt);
    }

    PanaudiumProcessor& proc;
    int layerIndex;
    juce::String prefix;
    juce::String layerName;

    // Layout boxes
    juce::Rectangle<int> oscBounds, filterBounds, pitchBounds, unisonBounds;
    juce::Rectangle<int> ampBounds, filtEnvBounds, modBounds, outputBounds;

    // Oscillator
    juce::ToggleButton onToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    juce::ComboBox engineCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> engineAttachment;
    PanaudiumKnob timbreKnob { "Timbre", true }, shapeKnob { "Shape", true }, charKnob { "Character", true };
    PanaudiumKnob noiseKnob { "Noise" }, fmDepthKnob { "FM Depth" }, fmRatioKnob { "FM Ratio" };

    // Pitch & Unison
    PanaudiumKnob octKnob { "Octave" }, semiKnob { "Semi" }, fineKnob { "Fine" };
    PanaudiumKnob uniVoicesKnob { "Voices" }, uniDetuneKnob { "Detune" }, uniSpreadKnob { "Spread" }, driftKnob { "Drift" };

    // Filter
    juce::ComboBox ftypeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ftypeAttachment;
    PanaudiumKnob cutoffKnob { "Cutoff", true }, resoKnob { "Resonance" }, driveKnob { "Drive" };
    PanaudiumKnob keyTrackKnob { "Key Track" }, filtEnvKnob { "Env Amt" };

    // Envelopes
    EnvelopeView ampEnvView { false };
    PanaudiumKnob ampAKnob { "Attack" }, ampDKnob { "Decay" }, ampSKnob { "Sustain" }, ampRKnob { "Release" };

    EnvelopeView filtEnvView { true };
    PanaudiumKnob filtAKnob { "Attack" }, filtDKnob { "Decay" }, filtSKnob { "Sustain" }, filtRKnob { "Release" };

    // Modulation
    PanaudiumKnob l1RateKnob { "Rate" }, l1PitchKnob { "> Pitch" }, l1CutKnob { "> Cutoff" };
    PanaudiumKnob l2RateKnob { "Rate" }, l2TimbreKnob { "> Timbre" }, l2PanKnob { "> Pan" };

    // Output
    PanaudiumKnob levelKnob { "Level" }, panKnob { "Pan" }, velSensKnob { "Vel Sens" };
};
