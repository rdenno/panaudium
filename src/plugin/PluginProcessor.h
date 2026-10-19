#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/Presets.h"
#include "../dsp/Synth.h"

class PanaudiumProcessor : public juce::AudioProcessor {
public:
    PanaudiumProcessor();
    ~PanaudiumProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return int(op::factoryPresets().size()); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::unique_ptr<op::Synth> synth;
    std::vector<std::atomic<float>*> raw;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanaudiumProcessor)
};
