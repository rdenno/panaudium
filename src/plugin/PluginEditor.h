#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ui/PanaudiumLookAndFeel.h"
#include "ui/PanaudiumKnob.h"
#include "ui/EnvelopeView.h"
#include "ui/LayerComponent.h"
#include "ui/FxRackComponent.h"

class PanaudiumEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PanaudiumEditor(PanaudiumProcessor&);
    ~PanaudiumEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Public test hooks (required, used by an automated screenshot tool)
    void showPage(int page);
    void showFxRack(int rack);

    void layoutMainContent();

private:
    void timerCallback() override;
    void setupPresetBrowser();
    void filterPresetsByCategory();
    void stepPreset(int delta);
    void syncPresetWithProcessor();
    void updateFooter();

    PanaudiumProcessor& proc;
    PanaudiumLookAndFeel lnf;

    // Design canvas scaled via AffineTransform
    class ContentContainer : public juce::Component {
    public:
        explicit ContentContainer(PanaudiumEditor& o) : owner(o) {}
        void paint(juce::Graphics& g) override;
        void resized() override;
    private:
        PanaudiumEditor& owner;
    };
    ContentContainer mainContent;

    // Header bar controls
    class Wordmark : public juce::Component {
    public:
        void paint(juce::Graphics& g) override;
    };
    Wordmark wordmark;

    juce::ComboBox categoryCombo;
    juce::ComboBox presetCombo;
    juce::TextButton prevBtn { "<" };
    juce::TextButton nextBtn { ">" };
    PanaudiumKnob masterGainKnob { "Master" };
    PanaudiumKnob glideKnob { "Glide" };

    // Page tabs
    juce::TextButton tabLayerA { "LAYER A" };
    juce::TextButton tabLayerB { "LAYER B" };
    juce::TextButton tabFxRack { "FX RACK" };

    // Page components
    LayerComponent layerAComp;
    LayerComponent layerBComp;
    FxRackComponent fxRackComp;

    // Footer
    juce::Label footerLabel;
    juce::Label sampleFreeLabel;

    int currentPage = 0;
    int lastKnownProgram = -1;

    struct PresetEntry {
        int index;
        juce::String name;
        juce::String category;
    };
    std::vector<PresetEntry> allPresets;
    std::vector<juce::String> categories;
    std::vector<int> filteredIndices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanaudiumEditor)
};
