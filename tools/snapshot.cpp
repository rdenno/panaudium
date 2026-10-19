// Renders the plugin editor's pages to PNG without a host, for reviewing the GUI.
// usage: panaudium_snapshot <outdir> [program index] [scale]
#include <juce_audio_utils/juce_audio_utils.h>
#include "../src/plugin/PluginEditor.h"
#include "../src/plugin/PluginProcessor.h"

static void settle() { juce::MessageManager::getInstance()->runDispatchLoopUntil(150); }

static void save(juce::Component& c, const juce::File& f, float scale) {
    const auto img = c.createComponentSnapshot(c.getLocalBounds(), true, scale);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat png;
    if (os.openedOk() && png.writeImageToStream(img, os)) std::printf("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: panaudium_snapshot <outdir> [program] [scale]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    out.createDirectory();
    const int program = argc > 2 ? juce::String(argv[2]).getIntValue() : 0;
    const float scale = argc > 3 ? juce::String(argv[3]).getFloatValue() : 1.0f;

    PanaudiumProcessor proc;
    proc.setCurrentProgram(program);
    std::unique_ptr<juce::AudioProcessorEditor> base(proc.createEditor());
    auto* ed = dynamic_cast<PanaudiumEditor*>(base.get());
    if (!ed) { std::printf("editor is not a PanaudiumEditor\n"); return 1; }
    settle();
    const char* names[] = { "layer_a", "layer_b" };
    for (int page = 0; page < 2; ++page) {
        ed->showPage(page); settle();
        save(*ed, out.getChildFile(juce::String("ui_") + names[page] + ".png"), scale);
    }
    const char* racks[] = { "fx_rack_a", "fx_rack_b", "fx_common" };
    for (int r = 0; r < 3; ++r) {
        ed->showPage(2); ed->showFxRack(r); settle();
        save(*ed, out.getChildFile(juce::String("ui_") + racks[r] + ".png"), scale);
    }
    // Host-driven program change while the editor is open: FX strips must swap units live.
    const int other = (program + 40) % proc.getNumPrograms();
    proc.setCurrentProgram(other);
    for (int i = 0; i < 4; ++i) settle();
    ed->showPage(2); ed->showFxRack(2); settle();
    save(*ed, out.getChildFile("ui_after_program_change.png"), scale);
    ed->setSize(ed->getWidth() * 3 / 4, ed->getHeight() * 3 / 4); settle();  // resizing must keep layout sane
    ed->showPage(0); settle();
    save(*ed, out.getChildFile("ui_layer_a_small.png"), scale);
    proc.editorBeingDeleted(ed);
    return 0;
}
