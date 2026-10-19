#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace op;

static juce::NormalisableRange<float> makeRange(const ParamDef& d) {
    if (d.isInt || !d.choices.empty()) return { d.min, d.max, 1.f };
    juce::NormalisableRange<float> r(d.min, d.max);
    if (d.centre > d.min && d.centre < d.max) r.setSkewForCentre(d.centre);
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout PanaudiumProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& d : buildParamDefs()) {
        const juce::ParameterID pid(d.id, 1);
        if (!d.choices.empty()) {
            juce::StringArray c; for (auto& s : d.choices) c.add(s);
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid, d.name, c, int(d.def)));
        } else if (d.isInt) {
            layout.add(std::make_unique<juce::AudioParameterInt>(pid, d.name, int(d.min), int(d.max), int(d.def)));
        } else {
            layout.add(std::make_unique<juce::AudioParameterFloat>(pid, d.name, makeRange(d), d.def));
        }
    }
    return layout;
}

PanaudiumProcessor::PanaudiumProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Panaudium", createLayout()),
      synth(std::make_unique<Synth>()) {
    for (const auto& d : buildParamDefs()) raw.push_back(apvts.getRawParameterValue(d.id));
    setCurrentProgram(0);
}

bool PanaudiumProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PanaudiumProcessor::prepareToPlay(double sampleRate, int) {
    synth = std::make_unique<Synth>();
    synth->prepare(float(sampleRate));
}

void PanaudiumProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    for (int i = 0; i < kNumParams; ++i) synth->setParam(i, raw[size_t(i)]->load());
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
    int pos = 0;
    const int n = buffer.getNumSamples();
    for (const auto meta : midi) {
        const int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > pos) { synth->process(L + pos, R + pos, at - pos); pos = at; }
        const auto m = meta.getMessage();
        if (m.isNoteOn()) synth->noteOn(m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) synth->noteOff(m.getNoteNumber());
        else if (m.isSustainPedalOn()) synth->setSustain(true);
        else if (m.isSustainPedalOff()) synth->setSustain(false);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth->allNotesOff();
    }
    if (pos < n) synth->process(L + pos, R + pos, n - pos);
    if (R == L && buffer.getNumChannels() > 1) buffer.copyFrom(1, 0, buffer, 0, 0, n);
}

void PanaudiumProcessor::setCurrentProgram(int index) {
    const auto& bank = factoryPresets();
    if (index < 0 || index >= int(bank.size())) return;
    currentProgram = index;
    const auto defs = buildParamDefs();
    for (int i = 0; i < kNumParams; ++i)
        if (auto* p = apvts.getParameter(defs[size_t(i)].id))
            p->setValueNotifyingHost(p->convertTo0to1(bank[size_t(index)].v[size_t(i)]));
}

const juce::String PanaudiumProcessor::getProgramName(int index) {
    const auto& bank = factoryPresets();
    if (index < 0 || index >= int(bank.size())) return {};
    return juce::String(bank[size_t(index)].category) + " - " + bank[size_t(index)].name;
}

void PanaudiumProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    state.setProperty("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void PanaudiumProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.hasType(apvts.state.getType())) {
            currentProgram = tree.getProperty("program", 0);
            apvts.replaceState(tree);
        }
    }
}

juce::AudioProcessorEditor* PanaudiumProcessor::createEditor() { return new PanaudiumEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PanaudiumProcessor(); }
