#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Readout boxes get explicit colours so they look the same whether they were created before or after the
// editor's LookAndFeel was applied (and when a strip rebuilds its knobs).
inline void styleTextBox(juce::Slider& s) {
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2b303b));
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0d0f13));
    s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd7dde8));
}

class PanaudiumKnob : public juce::Component {
public:
    PanaudiumKnob(const juce::String& name = {}, bool isLarge = false)
        : large(isLarge) {
        nameLabel.setText(name, juce::dontSendNotification);
        nameLabel.setJustificationType(juce::Justification::centred);
        nameLabel.setFont(juce::Font(juce::FontOptions(large ? 12.0f : 11.0f, juce::Font::bold)));
        nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(nameLabel);

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, large ? 72 : 56, 15);
        styleTextBox(slider);
        addAndMakeVisible(slider);
    }

    void setLabel(const juce::String& text) {
        nameLabel.setText(text, juce::dontSendNotification);
    }

    void setAccentColour(juce::Colour c) {
        slider.setColour(juce::Slider::rotarySliderFillColourId, c);
    }

    void setDimmed(bool dim) {
        slider.setEnabled(!dim);
        nameLabel.setEnabled(!dim);
        if (dim) {
            nameLabel.setText("-", juce::dontSendNotification);
        }
    }

    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId) {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
    }

    void setFormatFunction(std::function<juce::String(double)> fn) {
        slider.textFromValueFunction = std::move(fn);
        slider.updateText();   // refresh even when the value itself didn't change (e.g. FX unit swapped)
    }

    void resized() override {
        auto b = getLocalBounds();
        nameLabel.setBounds(b.removeFromTop(large ? 18 : 16));
        slider.setBounds(b);
    }

    juce::Slider slider;
    juce::Label nameLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool large = false;
};
