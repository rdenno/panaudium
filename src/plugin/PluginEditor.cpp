#include "PluginEditor.h"
#include <algorithm>

PanaudiumEditor::PanaudiumEditor(PanaudiumProcessor& p)
    : AudioProcessorEditor(p),
      proc(p),
      mainContent(*this),
      layerAComp(p, 0),
      layerBComp(p, 1),
      fxRackComp(p) {

    setLookAndFeel(&lnf);

    // Setup Master Gain & Glide knobs
    masterGainKnob.attach(proc.apvts, "g_gain");
    masterGainKnob.setAccentColour(juce::Colour(0xfff59e0b));
    masterGainKnob.setFormatFunction([](double v) {
        return (v > 0.0 ? "+" : "") + juce::String(v, 1) + " dB";
    });

    glideKnob.attach(proc.apvts, "g_glide");
    glideKnob.setAccentColour(juce::Colour(0xfff59e0b));
    glideKnob.setFormatFunction([](double v) {
        if (v < 0.001) return juce::String("Off");
        if (v < 1.0) return juce::String(juce::roundToInt(v * 1000.0)) + " ms";
        return juce::String(v, 2) + " s";
    });

    // Setup Page Tabs
    auto setupTab = [this](juce::TextButton& btn, int page) {
        btn.setClickingTogglesState(false);
        btn.onClick = [this, page] { showPage(page); };
        mainContent.addAndMakeVisible(btn);
    };
    setupTab(tabLayerA, 0);
    setupTab(tabLayerB, 1);
    setupTab(tabFxRack, 2);

    // Add controls to mainContent canvas
    mainContent.addAndMakeVisible(wordmark);
    mainContent.addAndMakeVisible(categoryCombo);
    mainContent.addAndMakeVisible(presetCombo);
    mainContent.addAndMakeVisible(prevBtn);
    mainContent.addAndMakeVisible(nextBtn);
    mainContent.addAndMakeVisible(glideKnob);
    mainContent.addAndMakeVisible(masterGainKnob);

    mainContent.addChildComponent(layerAComp);
    mainContent.addChildComponent(layerBComp);
    mainContent.addChildComponent(fxRackComp);

    // Setup Footer
    footerLabel.setFont(juce::Font(juce::FontOptions(11.5f)));
    footerLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    mainContent.addAndMakeVisible(footerLabel);

    sampleFreeLabel.setText("sample-free synthesis", juce::dontSendNotification);
    sampleFreeLabel.setJustificationType(juce::Justification::centredRight);
    sampleFreeLabel.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::italic)));
    sampleFreeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
    mainContent.addAndMakeVisible(sampleFreeLabel);

    addAndMakeVisible(mainContent);

    setupPresetBrowser();

    // Resizing & Aspect Ratio Constrainer
    setResizable(true, true);
    setResizeLimits(708, 456, 2360, 1520);
    if (auto* c = getConstrainer()) {
        c->setFixedAspectRatio(1180.0 / 760.0);
    }
    setSize(1180, 760);

    showPage(0);
    startTimerHz(30);
}

PanaudiumEditor::~PanaudiumEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void PanaudiumEditor::setupPresetBrowser() {
    const auto& bank = op::factoryPresets();
    allPresets.clear();
    categories.clear();
    categories.push_back("All");

    for (int i = 0; i < (int)bank.size(); ++i) {
        allPresets.push_back({ i, bank[i].name, bank[i].category });
        juce::String cat = bank[i].category;
        if (!cat.isEmpty() && std::find(categories.begin(), categories.end(), cat) == categories.end()) {
            categories.push_back(cat);
        }
    }

    categoryCombo.clear(juce::dontSendNotification);
    for (int i = 0; i < (int)categories.size(); ++i) {
        categoryCombo.addItem(categories[i], i + 1);
    }
    categoryCombo.setSelectedId(1, juce::dontSendNotification);

    categoryCombo.onChange = [this] { filterPresetsByCategory(); };

    presetCombo.onChange = [this] {
        int id = presetCombo.getSelectedId();
        if (id > 0) {
            int progIdx = id - 1;
            if (progIdx != proc.getCurrentProgram()) {
                proc.setCurrentProgram(progIdx);
                updateFooter();
            }
        }
    };

    prevBtn.onClick = [this] { stepPreset(-1); };
    nextBtn.onClick = [this] { stepPreset(1); };

    filterPresetsByCategory();
    syncPresetWithProcessor();
}

void PanaudiumEditor::filterPresetsByCategory() {
    int catIdx = categoryCombo.getSelectedId() - 1;
    juce::String selectedCat = (catIdx >= 0 && catIdx < (int)categories.size()) ? categories[catIdx] : "All";

    filteredIndices.clear();
    presetCombo.clear(juce::dontSendNotification);

    for (const auto& item : allPresets) {
        if (selectedCat == "All" || item.category == selectedCat) {
            filteredIndices.push_back(item.index);
            presetCombo.addItem(item.name, item.index + 1);
        }
    }

    int currentProg = proc.getCurrentProgram();
    presetCombo.setSelectedId(currentProg + 1, juce::dontSendNotification);
    updateFooter();
}

void PanaudiumEditor::stepPreset(int delta) {
    if (filteredIndices.empty()) return;
    int curProg = proc.getCurrentProgram();
    int currentPos = -1;
    for (int i = 0; i < (int)filteredIndices.size(); ++i) {
        if (filteredIndices[i] == curProg) {
            currentPos = i;
            break;
        }
    }

    int nextPos = 0;
    if (currentPos >= 0) {
        nextPos = (currentPos + delta + (int)filteredIndices.size()) % (int)filteredIndices.size();
    } else if (delta < 0) {
        nextPos = (int)filteredIndices.size() - 1;
    }
    int newProg = filteredIndices[nextPos];
    proc.setCurrentProgram(newProg);
    presetCombo.setSelectedId(newProg + 1, juce::dontSendNotification);
    updateFooter();
}

void PanaudiumEditor::syncPresetWithProcessor() {
    int curProg = proc.getCurrentProgram();
    if (curProg < 0 || curProg >= (int)allPresets.size()) return;

    if (curProg != lastKnownProgram) {
        lastKnownProgram = curProg;
        int catIdx = categoryCombo.getSelectedId() - 1;
        juce::String curCat = (catIdx >= 0 && catIdx < (int)categories.size()) ? categories[catIdx] : "All";
        if (curCat != "All" && allPresets[curProg].category != curCat) {
            for (int i = 0; i < (int)categories.size(); ++i) {
                if (categories[i] == allPresets[curProg].category) {
                    categoryCombo.setSelectedId(i + 1, juce::dontSendNotification);
                    break;
                }
            }
            filterPresetsByCategory();
        } else {
            presetCombo.setSelectedId(curProg + 1, juce::dontSendNotification);
        }
        updateFooter();
    }
}

void PanaudiumEditor::updateFooter() {
    int cur = proc.getCurrentProgram();
    if (cur >= 0 && cur < (int)allPresets.size()) {
        footerLabel.setText(allPresets[cur].category + "  -  " + allPresets[cur].name, juce::dontSendNotification);
    }
}

void PanaudiumEditor::showPage(int page) {
    currentPage = juce::jlimit(0, 2, page);
    tabLayerA.setToggleState(currentPage == 0, juce::dontSendNotification);
    tabLayerB.setToggleState(currentPage == 1, juce::dontSendNotification);
    tabFxRack.setToggleState(currentPage == 2, juce::dontSendNotification);

    layerAComp.setVisible(currentPage == 0);
    layerBComp.setVisible(currentPage == 1);
    fxRackComp.setVisible(currentPage == 2);

    layerAComp.resized();
    layerBComp.resized();
    fxRackComp.resized();
    mainContent.repaint();
}

void PanaudiumEditor::showFxRack(int rack) {
    showPage(2);
    fxRackComp.showRack(rack);
}

void PanaudiumEditor::timerCallback() {
    syncPresetWithProcessor();
    layerAComp.updateEngineLabels();
    layerBComp.updateEngineLabels();
    layerAComp.updateEnvelopes();
    layerBComp.updateEnvelopes();
    fxRackComp.pollSlotTypes();
}

void PanaudiumEditor::layoutMainContent() {
    // Header Bar (y = 0..58)
    wordmark.setBounds(16, 12, 190, 36);
    categoryCombo.setBounds(216, 16, 130, 26);
    prevBtn.setBounds(352, 16, 28, 26);
    presetCombo.setBounds(384, 16, 260, 26);
    nextBtn.setBounds(648, 16, 28, 26);

    glideKnob.setBounds(990, 4, 76, 52);
    masterGainKnob.setBounds(1076, 4, 76, 52);

    // Page Tabs (y = 60..94)
    tabLayerA.setBounds(16, 62, 140, 30);
    tabLayerB.setBounds(162, 62, 140, 30);
    tabFxRack.setBounds(308, 62, 150, 30);

    // Main Content Pages (y = 96..732)
    const auto pageArea = juce::Rectangle<int>(0, 96, 1180, 636);
    layerAComp.setBounds(pageArea);
    layerBComp.setBounds(pageArea);
    fxRackComp.setBounds(pageArea);

    // Footer (y = 732..760)
    footerLabel.setBounds(16, 736, 600, 20);
    sampleFreeLabel.setBounds(1180 - 250, 736, 234, 20);
}

void PanaudiumEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff121418));
}

void PanaudiumEditor::resized() {
    auto b = getLocalBounds();
    const float sX = (float)b.getWidth() / 1180.0f;
    const float sY = (float)b.getHeight() / 760.0f;
    const float s = std::min(sX, sY);
    const float ox = ((float)b.getWidth() - 1180.0f * s) * 0.5f;
    const float oy = ((float)b.getHeight() - 760.0f * s) * 0.5f;

    mainContent.setBounds(0, 0, 1180, 760);
    mainContent.setTransform(juce::AffineTransform::scale(s).translated(ox, oy));
}

void PanaudiumEditor::ContentContainer::paint(juce::Graphics& g) {
    // Fill main design surface
    g.fillAll(juce::Colour(0xff121418));

    // Header background
    const auto headerArea = juce::Rectangle<float>(0, 0, 1180, 58);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff181b22), 0.0f, 0.0f,
                                           juce::Colour(0xff13151a), 0.0f, 58.0f, false));
    g.fillRect(headerArea);
    g.setColour(juce::Colour(0xff262a34));
    g.drawHorizontalLine(58, 0.0f, 1180.0f);

    // Tabs background bar
    const auto tabsArea = juce::Rectangle<float>(0, 58, 1180, 38);
    g.setColour(juce::Colour(0xff15171d));
    g.fillRect(tabsArea);
    g.setColour(juce::Colour(0xff222630));
    g.drawHorizontalLine(96, 0.0f, 1180.0f);

    // Footer divider line
    g.setColour(juce::Colour(0xff222630));
    g.drawHorizontalLine(732, 0.0f, 1180.0f);
}

void PanaudiumEditor::ContentContainer::resized() {
    owner.layoutMainContent();
}

void PanaudiumEditor::Wordmark::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    // PANAUDIUM in gold
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffe8d9a8));
    g.drawText("P A N A U D I U M", b.removeFromTop(20.0f), juce::Justification::left, true);

    // Subtext SYNTHESIZER
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
    g.setColour(juce::Colour(0xff64748b));
    g.drawText("S Y N T H E S I Z E R", b, juce::Justification::left, true);
}
