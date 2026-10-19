// Headless "DAW" test: loads the built Panaudium.vst3 through JUCE's VST3 hosting (the same path a DAW
// uses), then exercises scanning, instantiation, programs, MIDI playback, automation, state round-trips,
// odd block sizes, sample-rate changes, the editor, and real-time CPU margin.
//
// usage: panaudium_hosttest <path/to/Panaudium.vst3> <outdir> [--all-programs]
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <chrono>
#include <cstdio>

namespace {

int failures = 0;
void check(bool ok, const juce::String& what) {
    std::printf("[%s] %s\n", ok ? " ok " : "FAIL", what.toRawUTF8());
    if (!ok) ++failures;
}

struct RenderStats { float peak = 0; bool finite = true; double maxBlockMs = 0, avgBlockMs = 0; };

// Plays a short chord phrase into the plugin and returns the audio.
RenderStats render(juce::AudioPluginInstance& p, double sr, int block, double seconds, juce::AudioBuffer<float>& out,
                   bool automate = false, bool variableBlocks = false) {
    p.prepareToPlay(sr, block);
    const int total = int(seconds * sr);
    out.setSize(2, total);
    out.clear();
    juce::Random rng(1234);
    RenderStats st;
    int pos = 0, blocks = 0;
    double sumMs = 0;
    const int notes[] = { 48, 55, 60, 64, 67 };
    auto& params = p.getParameters();
    while (pos < total) {
        int n = variableBlocks ? 1 + rng.nextInt(block) : block;
        n = std::min(n, total - pos);
        juce::AudioBuffer<float> buf(2, n);
        buf.clear();
        juce::MidiBuffer midi;
        for (int k = 0; k < 5; ++k) {
            const int on = int((0.05 + 0.15 * k) * sr), off = int((seconds * 0.6) * sr);
            if (on >= pos && on < pos + n) midi.addEvent(juce::MidiMessage::noteOn(1, notes[k], 0.8f), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent(juce::MidiMessage::noteOff(1, notes[k]), off - pos);
        }
        if (automate && (blocks % 2) == 0) {
            // Random automation of the synth's own parameters (not JUCE's MIDI-CC emulation parameters),
            // skipping the master gain so the bounded-output check stays meaningful.
            static std::vector<juce::AudioProcessorParameter*> pool;
            if (pool.empty())
                for (auto* prm : params) {
                    const auto name = prm->getName(64);
                    if (!name.startsWithIgnoreCase("MIDI CC") && !name.containsIgnoreCase("Master Gain")) pool.push_back(prm);
                }
            if (!pool.empty()) pool[size_t(rng.nextInt(int(pool.size())))]->setValue(rng.nextFloat());
        }
        const auto t0 = std::chrono::steady_clock::now();
        p.processBlock(buf, midi);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        st.maxBlockMs = std::max(st.maxBlockMs, ms);
        sumMs += ms;
        for (int c = 0; c < 2; ++c) {
            out.copyFrom(c, pos, buf, c, 0, n);
            for (int i = 0; i < n; ++i) {
                const float v = buf.getSample(c, i);
                if (!std::isfinite(v)) st.finite = false; else st.peak = std::max(st.peak, std::abs(v));
            }
        }
        pos += n; ++blocks;
    }
    st.avgBlockMs = sumMs / std::max(1, blocks);
    p.releaseResources();
    return st;
}

void writeWav(const juce::File& f, const juce::AudioBuffer<float>& b, double sr) {
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os(f.createOutputStream());
    if (!os) return;
    if (auto* w = wav.createWriterFor(os.get(), sr, 2, 24, {}, 0)) {
        os.release();
        std::unique_ptr<juce::AudioFormatWriter> writer(w);
        writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
    }
}

float maxDiff(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b) {
    float d = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < std::min(a.getNumSamples(), b.getNumSamples()); ++i)
            d = std::max(d, std::abs(a.getSample(c, i) - b.getSample(c, i)));
    return d;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) { std::printf("usage: panaudium_hosttest <Panaudium.vst3> <outdir> [--all-programs] [--no-perf]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String path = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getFullPathName();
    const juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);
    outDir.createDirectory();
    bool allPrograms = false, perf = true;   // --no-perf: skip timing checks (shared CI runners have noisy timing)

    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> types;
    fmt.findAllTypesForFile(types, path);
    check(types.size() == 1, "scan finds exactly one plugin in " + path);
    if (types.isEmpty()) return 1;
    const auto& desc = *types[0];
    check(desc.isInstrument, "registered as an instrument (category: " + desc.category + ")");


    juce::String err;
    auto plugin = fmt.createInstanceFromDescription(desc, 48000.0, 512, err);
    check(plugin != nullptr, "instantiates" + (err.isNotEmpty() ? " (" + err + ")" : juce::String()));
    if (!plugin) return 1;
    std::printf("      %s by %s, %d parameters, %d programs\n", desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(), plugin->getParameters().size(), plugin->getNumPrograms());
    check(plugin->getNumPrograms() > 0, "exposes factory programs");
    check(plugin->getTotalNumOutputChannels() == 2 && plugin->getTotalNumInputChannels() == 0, "stereo output, no audio input (" + juce::String(plugin->getTotalNumOutputChannels()) + " out)");

    // Programs: render each (or a spread) and check for silence/NaN/clipping and real-time margin.
    const int np = plugin->getNumPrograms();
    std::vector<int> progs;
    if (allPrograms) for (int i = 0; i < np; ++i) progs.push_back(i);
    else for (int i = 0; i < np; i += std::max(1, np / 12)) progs.push_back(i);
    double worstRt = 1e9; juce::String worstName;
    for (int i : progs) {
        plugin->setCurrentProgram(i);
        juce::AudioBuffer<float> out;
        const auto st = render(*plugin, 48000.0, 512, 3.0, out);
        const auto name = plugin->getProgramName(i);
        const double rt = (512.0 / 48000.0 * 1000.0) / std::max(1e-6, st.maxBlockMs);
        if (rt < worstRt) { worstRt = rt; worstName = name; }
        check(st.finite && st.peak > 1e-4f && st.peak <= 1.0001f,
              "program " + juce::String(i).paddedLeft(' ', 3) + " " + name.paddedRight(' ', 34) + juce::String::formatted(" peak %.3f  worst block %.2f ms (%.0fx realtime headroom)", st.peak, st.maxBlockMs, rt));
        writeWav(outDir.getChildFile(juce::String::formatted("host_prog_%03d.wav", i)), out, 48000.0);
    }
    if (perf) check(worstRt > 2.0, "worst-case block leaves >2x real-time headroom (worst: " + worstName + ")");

    // State round trip: save state, load into a fresh instance, renders must match.
    plugin->setCurrentProgram(std::min(5, np - 1));
    juce::MemoryBlock state;
    plugin->getStateInformation(state);
    juce::AudioBuffer<float> a, b;
    render(*plugin, 48000.0, 512, 2.0, a);
    auto second = fmt.createInstanceFromDescription(desc, 48000.0, 512, err);
    second->setStateInformation(state.getData(), int(state.getSize()));
    render(*second, 48000.0, 512, 2.0, b);
    check(maxDiff(a, b) < 1e-4f, juce::String::formatted("state save/restore reproduces the sound (max diff %.2g)", maxDiff(a, b)));

    // Robustness: automation storm, ragged block sizes, other sample rates.
    juce::AudioBuffer<float> out;
    auto st = render(*plugin, 48000.0, 512, 4.0, out, true);
    check(st.finite && st.peak <= 1.0001f, "random parameter automation stays finite and bounded");
    st = render(*plugin, 48000.0, 1024, 3.0, out, false, true);
    check(st.finite && st.peak > 1e-4f, "variable block sizes 1..1024");
    for (double sr : { 44100.0, 88200.0, 96000.0, 192000.0 }) {
        st = render(*plugin, sr, 256, 2.0, out);
        check(st.finite && st.peak > 1e-4f && st.peak <= 1.0001f, juce::String::formatted("sample rate %.0f Hz", sr));
    }

    // Editor open/close (the GUI must construct and tear down cleanly inside a host).
    for (int i = 0; i < 3; ++i) {
        std::unique_ptr<juce::AudioProcessorEditor> ed(plugin->createEditorIfNeeded());
        check(ed != nullptr && ed->getWidth() > 200 && ed->getHeight() > 200,
              juce::String::formatted("editor opens (%dx%d)", ed ? ed->getWidth() : 0, ed ? ed->getHeight() : 0));
        if (ed) plugin->editorBeingDeleted(ed.get());
    }

    std::printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 2 : 0;
}
