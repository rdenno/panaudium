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
    static const auto defs = buildParamDefs();
    for (const auto& d : defs) {
        const juce::ParameterID pid(d.id, 1);
        if (!d.choices.empty()) {
            juce::StringArray c; for (auto& s : d.choices) c.add(s);
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid, d.name, c, int(d.def)));
        } else if (d.isInt) {
            layout.add(std::make_unique<juce::AudioParameterInt>(pid, d.name, int(d.min), int(d.max), int(d.def)));
        } else {
            const int index = int(&d - defs.data());
            auto attrs = juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([this, index](float v, int) { return paramText(index, v); })
                .withValueFromStringFunction([this, index](const juce::String& t) { return paramFromText(index, t); });
            layout.add(std::make_unique<juce::AudioParameterFloat>(pid, d.name, makeRange(d), d.def, attrs));
        }
    }
    return layout;
}

// FX slot knobs (index >= kFxParamBase, slot param >= S_K1) format through the unit currently in their slot.
static int fxKnobOf(int index, int& slot) {
    if (index < kFxParamBase) return -1;
    slot = (index - kFxParamBase) / S_NumSlotParams;
    const int p = (index - kFxParamBase) % S_NumSlotParams;
    return p >= S_K1 ? p - S_K1 : -1;
}

juce::String PanaudiumProcessor::paramText(int index, float value) const {
    static const auto defs = buildParamDefs();
    int slot = 0;
    const int k = fxKnobOf(index, slot);
    if (k >= 0) {
        const int type = raw.size() > size_t(fxp(slot, S_Type)) ? int(raw[size_t(fxp(slot, S_Type))]->load() + 0.5f) : 0;
        const FxKnob& kn = fxKnob(type, k);
        if (!kn.name) return "-";
        return formatValue(kn.value(value), kn.unit);
    }
    return formatValue(value, defs[size_t(index)].unit);
}

float PanaudiumProcessor::paramFromText(int index, const juce::String& text) const {
    static const auto defs = buildParamDefs();
    int slot = 0;
    const int k = fxKnobOf(index, slot);
    if (k >= 0) {
        const int type = raw.size() > size_t(fxp(slot, S_Type)) ? int(raw[size_t(fxp(slot, S_Type))]->load() + 0.5f) : 0;
        const FxKnob& kn = fxKnob(type, k);
        return kn.name ? kn.norm(parseValue(text.toStdString(), kn.unit)) : 0.5f;
    }
    return parseValue(text.toStdString(), defs[size_t(index)].unit);
}

PanaudiumProcessor::PanaudiumProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Panaudium", createLayout()),
      synth(std::make_unique<Synth>()) {
    for (const auto& d : buildParamDefs()) raw.push_back(apvts.getRawParameterValue(d.id));
    liveSynth = synth.get();
    setCurrentProgram(0);
    startTimerHz(20);
}

void PanaudiumProcessor::preloadCurrentFx() {
    if (auto* s = liveSynth.load()) {
        float p[kNumParams];
        for (int i = 0; i < kNumParams; ++i) p[i] = raw[size_t(i)]->load();
        s->preloadFx(p);
    }
}

bool PanaudiumProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PanaudiumProcessor::prepareToPlay(double sampleRate, int) {
    auto fresh = std::make_unique<Synth>();
    for (int i = 0; i < kNumParams; ++i) fresh->setParam(i, raw[size_t(i)]->load());
    fresh->prepare(float(sampleRate));   // also preloads the FX units the current settings use
    { float p[kNumParams]; for (int i = 0; i < kNumParams; ++i) p[i] = raw[size_t(i)]->load(); fresh->primeFx(p); }
    liveSynth = fresh.get();
    synth = std::move(fresh);
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
    if (auto* s = liveSynth.load()) s->primeFx(bank[size_t(index)].v.data());   // builds IRs etc. off the audio thread
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
            if (auto* s = liveSynth.load()) { float p[kNumParams]; for (int i = 0; i < kNumParams; ++i) p[i] = raw[size_t(i)]->load(); s->primeFx(p); }
        }
    }
}

juce::AudioProcessorEditor* PanaudiumProcessor::createEditor() { return new PanaudiumEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PanaudiumProcessor(); }
