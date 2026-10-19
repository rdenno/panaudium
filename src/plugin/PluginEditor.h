#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Preset browser bar on top of JUCE's generic parameter editor.
class PanaudiumEditor : public juce::AudioProcessorEditor {
public:
    explicit PanaudiumEditor(PanaudiumProcessor& p) : AudioProcessorEditor(p), proc(p), generic(p) {
        title.setText("Panaudium  -  sample-free layered synth", juce::dontSendNotification);
        title.setFont(juce::FontOptions(18.f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, juce::Colour(0xffe8d9a8));
        addAndMakeVisible(title);
        for (int i = 0; i < p.getNumPrograms(); ++i) presets.addItem(p.getProgramName(i), i + 1);
        presets.setSelectedId(p.getCurrentProgram() + 1, juce::dontSendNotification);
        presets.onChange = [this] { proc.setCurrentProgram(presets.getSelectedId() - 1); };
        prev.onClick = [this] { step(-1); };
        next.onClick = [this] { step(1); };
        addAndMakeVisible(presets); addAndMakeVisible(prev); addAndMakeVisible(next);
        addAndMakeVisible(generic);
        setResizable(true, true);
        setSize(760, 720);
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff1b1d24)); }
    void resized() override {
        auto r = getLocalBounds().reduced(8);
        auto top = r.removeFromTop(32);
        title.setBounds(top.removeFromLeft(320));
        next.setBounds(top.removeFromRight(32)); prev.setBounds(top.removeFromRight(32));
        presets.setBounds(top.reduced(4, 2));
        r.removeFromTop(6);
        generic.setBounds(r);
    }

private:
    void step(int d) {
        const int n = proc.getNumPrograms();
        const int idx = (proc.getCurrentProgram() + d + n) % n;
        presets.setSelectedId(idx + 1, juce::sendNotificationSync);
    }
    PanaudiumProcessor& proc;
    juce::GenericAudioProcessorEditor generic;
    juce::Label title;
    juce::ComboBox presets;
    juce::TextButton prev { "<" }, next { ">" };
};
