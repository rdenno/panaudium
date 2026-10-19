#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PanaudiumKnob.h"
#include "../../dsp/Params.h"
#include "../PluginProcessor.h"

struct FxTheme {
    juce::Colour bg;
    juce::Colour accent;
    const char* displayName;
};

inline FxTheme getFxTheme(int type) {
    switch (type) {
        case op::FX_Stompbox:   return { juce::Colour(0xff2d170e), juce::Colour(0xffe06a20), "STOMPBOX" };
        case op::FX_Phaser:     return { juce::Colour(0xff291016), juce::Colour(0xffb8263f), "ANALOG PHASER" };
        case op::FX_Chorus:     return { juce::Colour(0xff0e2229), juce::Colour(0xff289bb0), "CHORUS" };
        case op::FX_Flanger:    return { juce::Colour(0xff22112b), juce::Colour(0xff893ba6), "FLANGER" };
        case op::FX_Tremolo:    return { juce::Colour(0xff2b1e0d), juce::Colour(0xffd99318), "TREMOLO" };
        case op::FX_Rotary:     return { juce::Colour(0xff261811), juce::Colour(0xff8c5c36), "ROTARY" };
        case op::FX_Delay:      return { juce::Colour(0xff111f2d), juce::Colour(0xff4887b5), "DELAY" };
        case op::FX_ProVerb:    return { juce::Colour(0xff290f0f), juce::Colour(0xffc42b2b), "PRO-VERB" };
        case op::FX_Innerspace: return { juce::Colour(0xff0c2420), juce::Colour(0xff1f8f7c), "INNERSPACE" };
        case op::FX_Shimmer:    return { juce::Colour(0xff0c242b), juce::Colour(0xff2ad1e8), "SHIMMER" };
        case op::FX_EQ:         return { juce::Colour(0xff1d2126), juce::Colour(0xff7e8c9c), "STUDIO EQ" };
        case op::FX_Compressor: return { juce::Colour(0xff16202c), juce::Colour(0xff5b7c9e), "COMPRESSOR" };
        case op::FX_LoFi:       return { juce::Colour(0xff172912), juce::Colour(0xff5bc23c), "LO-FI" };
        case op::FX_RingShift:  return { juce::Colour(0xff2d1025), juce::Colour(0xffd92e9e), "RING / SHIFT" };
        case op::FX_Wah:        return { juce::Colour(0xff2a230d), juce::Colour(0xffdfb326), "WAH / ENV" };
        case op::FX_Tape:       return { juce::Colour(0xff261c14), juce::Colour(0xffb58b62), "TAPE" };
        case op::FX_Amp:        return { juce::Colour(0xff211c15), juce::Colour(0xffd4af37), "AMP SIM" };
        case op::FX_Off:
        default:                return { juce::Colour(0xff15171d), juce::Colour(0xff404654), "EMPTY SLOT" };
    }
}

inline juce::String formatFxValue(float val, const char* unit) {
    if (unit == nullptr || *unit == '\0') {
        return juce::String(juce::roundToInt(val));
    }
    juce::String u(unit);
    if (u == "%") {
        return juce::String(juce::roundToInt(val * 100.0f)) + " %";
    }
    if (u == "Hz") {
        if (val >= 1000.0f) return juce::String(val * 0.001f, 1) + " kHz";
        if (val >= 10.0f) return juce::String(juce::roundToInt(val)) + " Hz";
        return juce::String(val, 2) + " Hz";
    }
    if (u == "ms") {
        if (val >= 10.0f) return juce::String(juce::roundToInt(val)) + " ms";
        return juce::String(val, 1) + " ms";
    }
    if (u == "s") {
        return juce::String(val, 2) + " s";
    }
    if (u == "dB") {
        return (val > 0.0f ? "+" : "") + juce::String(val, 1) + " dB";
    }
    if (u == "x") {
        return juce::String(val, 1) + " x";
    }
    if (u == "st") {
        return juce::String(juce::roundToInt(val)) + " st";
    }
    if (u == "ct") {
        return juce::String(juce::roundToInt(val)) + " ct";
    }
    if (u == "deg") {
        return juce::String(juce::roundToInt(val)) + juce::String(juce::CharPointer_UTF8("\xc2\xb0"));
    }
    if (u == "oct") {
        return juce::String(val, 1) + " oct";
    }
    return juce::String(val, 1) + " " + u;
}

class FxRackComponent;

class FxSlotStrip : public juce::Component {
public:
    FxSlotStrip(PanaudiumProcessor& processor, FxRackComponent& owner, int rIndex, int sIndex)
        : proc(processor), rackComp(owner), rackIndex(rIndex), slotIndex(sIndex) {

        static const char* rackTag[op::R_NumRacks] = { "A", "B", "C" };
        prefix = "fx" + juce::String(rackTag[rackIndex]) + juce::String(slotIndex + 1) + "_";

        // Unit Button
        unitBtn.setButtonText("OFF");
        unitBtn.onClick = [this] { openUnitMenu(); };
        addAndMakeVisible(unitBtn);

        // Power LED Toggle
        powerBtn.setButtonText("");
        addAndMakeVisible(powerBtn);
        powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, prefix + "on", powerBtn);

        // Mix Slider
        mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 16);
        styleTextBox(mixSlider);
        mixSlider.textFromValueFunction = [](double v) {
            return juce::String(juce::roundToInt(v * 100.0)) + "%";
        };
        addAndMakeVisible(mixSlider);
        mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, prefix + "mix", mixSlider);

        // Model ComboBox & Label
        modelLabel.setText("Model", juce::dontSendNotification);
        modelLabel.setJustificationType(juce::Justification::centred);
        modelLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        modelLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addChildComponent(modelLabel);

        modelCombo.setJustificationType(juce::Justification::centredLeft);
        addChildComponent(modelCombo);

        // 6 Knobs
        for (int k = 0; k < 6; ++k) {
            knobs[k] = std::make_unique<PanaudiumKnob>("Knob");
            setupKnob(*knobs[k], prefix + "k" + juce::String(k + 1));
            addChildComponent(*knobs[k]);
        }

        // Up/Down buttons
        upBtn.onClick = [this] { moveSlot(-1); };
        downBtn.onClick = [this] { moveSlot(1); };
        addAndMakeVisible(upBtn);
        addAndMakeVisible(downBtn);

        upBtn.setEnabled(slotIndex > 0);
        downBtn.setEnabled(slotIndex < op::kRackSlots[rackIndex] - 1);

        checkTypeAndUpdate(true);
    }

    void openUnitMenu() {
        juce::PopupMenu m;
        const auto& names = op::fxTypeNames();
        for (int i = 0; i < (int)names.size(); ++i) {
            m.addItem(i + 1, names[i], true, i == lastKnownType);
        }
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&unitBtn), [this](int result) {
            if (result <= 0) return;
            setNewType(result - 1);
        });
    }

    void setNewType(int newType) {
        const auto& info = op::fxInfo(newType);

        if (auto* p = proc.apvts.getParameter(prefix + "type")) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1((float)newType));
            p->endChangeGesture();
        }
        if (auto* p = proc.apvts.getParameter(prefix + "on")) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(1.0f);
            p->endChangeGesture();
        }
        if (auto* p = proc.apvts.getParameter(prefix + "mix")) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(info.defaultMix);
            p->endChangeGesture();
        }
        if (auto* p = proc.apvts.getParameter(prefix + "model")) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(0.0f));
            p->endChangeGesture();
        }
        for (int k = 0; k < 6; ++k) {
            if (auto* p = proc.apvts.getParameter(prefix + "k" + juce::String(k + 1))) {
                float defVal = (newType == op::FX_Off || !info.knobs[k].name) ? 0.5f : info.knobs[k].defNorm();
                p->beginChangeGesture();
                p->setValueNotifyingHost(defVal);
                p->endChangeGesture();
            }
        }

        updateSlotUI(newType);
    }

    void checkTypeAndUpdate(bool force = false) {
        int curType = op::FX_Off;
        if (auto* p = proc.apvts.getParameter(prefix + "type")) {
            curType = juce::roundToInt(p->convertFrom0to1(p->getValue()));
        }
        if (curType != lastKnownType || force) {
            updateSlotUI(curType);
        } else {
            // Keep model selection in sync if host automated it
            if (auto* p = proc.apvts.getParameter(prefix + "model")) {
                int curModel = juce::roundToInt(p->convertFrom0to1(p->getValue()));
                if (curModel >= 0 && curModel < modelCombo.getNumItems() &&
                    modelCombo.getSelectedItemIndex() != curModel) {
                    modelCombo.setSelectedItemIndex(curModel, juce::dontSendNotification);
                }
            }
        }
    }

    void updateSlotUI(int type) {
        lastKnownType = type;
        const auto& info = op::fxInfo(type);
        const auto theme = getFxTheme(type);

        // Update Unit Button
        unitBtn.setButtonText(type == op::FX_Off ? "EMPTY SLOT" : info.name);
        unitBtn.setColour(juce::TextButton::buttonColourId, theme.bg.brighter(0.12f));
        unitBtn.setColour(juce::TextButton::textColourOffId, theme.accent.brighter(0.2f));

        // Update Mix Slider accent
        mixSlider.setColour(juce::Slider::rotarySliderFillColourId, theme.accent);

        // Recreate model combo items before reattaching!
        modelAttachment.reset();
        modelCombo.clear(juce::dontSendNotification);

        if (type != op::FX_Off && info.models.size() > 1) {
            for (int i = 0; i < (int)info.models.size(); ++i) {
                modelCombo.addItem(info.models[i], i + 1);
            }
            modelCombo.setVisible(true);
            modelLabel.setVisible(true);

            // Fetch current model
            int curModel = 0;
            if (auto* p = proc.apvts.getParameter(prefix + "model")) {
                curModel = juce::roundToInt(p->convertFrom0to1(p->getValue()));
            }
            modelCombo.setSelectedItemIndex(curModel, juce::dontSendNotification);

            modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                proc.apvts, prefix + "model", modelCombo);

            modelCombo.onChange = [this, type] {
                int idx = modelCombo.getSelectedItemIndex();
                if (idx >= 0) {
                    if (auto* p = proc.apvts.getParameter(prefix + "model")) {
                        p->beginChangeGesture();
                        p->setValueNotifyingHost(p->convertTo0to1((float)idx));
                        p->endChangeGesture();
                    }
                }
            };
        } else {
            modelCombo.setVisible(false);
            modelLabel.setVisible(false);
        }

        // Update Knobs
        for (int k = 0; k < 6; ++k) {
            const auto& kn = info.knobs[k];
            if (type != op::FX_Off && kn.name != nullptr) {
                knobs[k]->setVisible(true);
                knobs[k]->setLabel(kn.name);
                knobs[k]->setAccentColour(theme.accent);
                knobs[k]->setFormatFunction([type, k](double norm) {
                    const auto& knobDef = op::fxKnob(type, k);
                    if (!knobDef.name) return juce::String();
                    return formatFxValue(knobDef.value((float)norm), knobDef.unit);
                });
            } else {
                knobs[k]->setVisible(false);
            }
        }

        resized();
        repaint();
    }

    void moveSlot(int delta);

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const auto theme = getFxTheme(lastKnownType);
        const bool isOn = powerBtn.getToggleState() && lastKnownType != op::FX_Off;

        // Background gradient
        juce::Colour bgLeft = isOn ? theme.bg : juce::Colour(0xff14161c);
        juce::Colour bgRight = isOn ? theme.bg.darker(0.35f) : juce::Colour(0xff101216);

        g.setGradientFill(juce::ColourGradient(bgLeft, bounds.getX(), bounds.getY(),
                                               bgRight, bounds.getRight(), bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, 6.0f);

        // Border
        juce::Colour borderCol = isOn ? theme.accent.withAlpha(0.45f) : juce::Colour(0xff252932);
        g.setColour(borderCol);
        g.drawRoundedRectangle(bounds, 6.0f, 1.2f);

        // Themed stylized title in decorative right area
        if (lastKnownType != op::FX_Off) {
            g.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
            g.setColour(theme.accent.withAlpha(isOn ? 0.22f : 0.08f));
            g.drawText(theme.displayName,
                       (int)(bounds.getRight() - 280.0f), (int)(bounds.getY() + 16.0f),
                       240, 36, juce::Justification::centredRight, true);
        } else {
            g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
            g.setColour(juce::Colour(0x20ffffff));
            g.drawText("EMPTY SLOT",
                       (int)(bounds.getX() + 220.0f), (int)(bounds.getCentreY() - 14.0f),
                       200, 28, juce::Justification::centredLeft, true);
        }

        // Divider between left controls and middle knobs
        g.setColour(juce::Colour(0x18ffffff));
        g.drawVerticalLine(196, bounds.getY() + 8.0f, bounds.getBottom() - 8.0f);
    }

    void resized() override {
        const auto b = getLocalBounds();

        // Left section
        unitBtn.setBounds(14, 14, 140, 36);
        powerBtn.setBounds(160, 20, 24, 24);
        mixSlider.setBounds(14, 66, 172, 28);

        // Model section
        const bool hasModel = modelCombo.isVisible();
        if (hasModel) {
            modelLabel.setBounds(216, 20, 136, 16);
            modelCombo.setBounds(216, 42, 136, 26);
        }

        // Knobs section
        const int startX = hasModel ? 368 : 220;
        const int knobY = 14;
        const int knobW = 84;
        const int knobH = 104;
        int visibleIndex = 0;

        for (int k = 0; k < 6; ++k) {
            if (knobs[k]->isVisible()) {
                knobs[k]->setBounds(startX + visibleIndex * 90, knobY, knobW, knobH);
                visibleIndex++;
            }
        }

        // Right edge: Up and Down buttons
        const int btnW = 26;
        const int btnH = 42;
        const int rightX = b.getWidth() - 36;
        upBtn.setBounds(rightX, 16, btnW, btnH);
        downBtn.setBounds(rightX, 68, btnW, btnH);
    }

private:
    void setupKnob(PanaudiumKnob& knob, const juce::String& paramId) {
        knob.attach(proc.apvts, paramId);
    }

    PanaudiumProcessor& proc;
    FxRackComponent& rackComp;
    int rackIndex;
    int slotIndex;
    juce::String prefix;
    int lastKnownType = -1;

    juce::TextButton unitBtn;
    juce::ToggleButton powerBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;

    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    juce::Label modelLabel;
    juce::ComboBox modelCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;

    std::array<std::unique_ptr<PanaudiumKnob>, 6> knobs;

    juce::TextButton upBtn { juce::CharPointer_UTF8("\xe2\x96\xb2") };
    juce::TextButton downBtn { juce::CharPointer_UTF8("\xe2\x96\xbc") };
};

class FxRackComponent : public juce::Component {
public:
    FxRackComponent(PanaudiumProcessor& processor)
        : proc(processor) {

        // Sub-tabs: RACK A | RACK B | COMMON
        setupTab(subTabA, "RACK A (LAYER A)", 0);
        setupTab(subTabB, "RACK B (LAYER B)", 1);
        setupTab(subTabCommon, "COMMON RACK (OUTPUT)", 2);

        // Rack A (3 slots)
        for (int i = 0; i < op::kRackSlots[op::R_A]; ++i) {
            rackStrips[op::R_A].push_back(std::make_unique<FxSlotStrip>(proc, *this, op::R_A, i));
            addChildComponent(*rackStrips[op::R_A].back());
        }

        // Rack B (3 slots)
        for (int i = 0; i < op::kRackSlots[op::R_B]; ++i) {
            rackStrips[op::R_B].push_back(std::make_unique<FxSlotStrip>(proc, *this, op::R_B, i));
            addChildComponent(*rackStrips[op::R_B].back());
        }

        // Common (4 slots)
        for (int i = 0; i < op::kRackSlots[op::R_Common]; ++i) {
            rackStrips[op::R_Common].push_back(std::make_unique<FxSlotStrip>(proc, *this, op::R_Common, i));
            addChildComponent(*rackStrips[op::R_Common].back());
        }

        showRack(2); // Default to COMMON rack
    }

    void showRack(int rack) {
        currentRack = juce::jlimit(0, 2, rack);
        subTabA.setToggleState(currentRack == 0, juce::dontSendNotification);
        subTabB.setToggleState(currentRack == 1, juce::dontSendNotification);
        subTabCommon.setToggleState(currentRack == 2, juce::dontSendNotification);

        for (int r = 0; r < op::R_NumRacks; ++r) {
            for (auto& strip : rackStrips[r]) {
                strip->setVisible(r == currentRack);
            }
        }
        resized();
        repaint();
    }

    void swapSlots(int r, int slotA, int slotB) {
        if (slotA < 0 || slotB < 0 || slotA == slotB) return;
        const int maxSlots = op::kRackSlots[r];
        if (slotA >= maxSlots || slotB >= maxSlots) return;

        static const char* rackTag[op::R_NumRacks] = { "A", "B", "C" };
        const juce::String pfxA = "fx" + juce::String(rackTag[r]) + juce::String(slotA + 1) + "_";
        const juce::String pfxB = "fx" + juce::String(rackTag[r]) + juce::String(slotB + 1) + "_";

        const char* params[10] = { "type", "on", "mix", "model", "k1", "k2", "k3", "k4", "k5", "k6" };
        for (const char* pName : params) {
            auto* pA = proc.apvts.getParameter(pfxA + pName);
            auto* pB = proc.apvts.getParameter(pfxB + pName);
            if (pA && pB) {
                const float valA = pA->getValue();
                const float valB = pB->getValue();
                pA->beginChangeGesture();
                pB->beginChangeGesture();
                pA->setValueNotifyingHost(valB);
                pB->setValueNotifyingHost(valA);
                pA->endChangeGesture();
                pB->endChangeGesture();
            }
        }

        rackStrips[r][size_t(slotA)]->checkTypeAndUpdate(true);
        rackStrips[r][size_t(slotB)]->checkTypeAndUpdate(true);
    }

    void pollSlotTypes() {
        for (int r = 0; r < op::R_NumRacks; ++r) {
            for (auto& strip : rackStrips[r]) {
                strip->checkTypeAndUpdate(false);
            }
        }
    }

    void paint(juce::Graphics& g) override {
        // Subtle background behind sub-tabs
        g.setColour(juce::Colour(0xff121418));
        g.fillRect(getLocalBounds());
    }

    void resized() override {
        // Sub-tabs row
        const int tabY = 8;
        const int tabH = 30;
        const int tabW = 160;
        subTabA.setBounds(14, tabY, tabW, tabH);
        subTabB.setBounds(180, tabY, tabW, tabH);
        subTabCommon.setBounds(346, tabY, 190, tabH);

        // Strips area (from y = 46 to bottom)
        const int topY = 46;
        const int stripGap = 8;
        const int totalH = getHeight() - topY - 10;
        const int numSlots = op::kRackSlots[currentRack];
        const int stripH = juce::jlimit(110, 134, (totalH - (numSlots - 1) * stripGap) / numSlots);

        for (int i = 0; i < numSlots; ++i) {
            rackStrips[currentRack][size_t(i)]->setBounds(14, topY + i * (stripH + stripGap), getWidth() - 28, stripH);
        }
    }

private:
    void setupTab(juce::TextButton& btn, const juce::String& text, int rackIdx) {
        btn.setButtonText(text);
        btn.setClickingTogglesState(false);
        btn.onClick = [this, rackIdx] { showRack(rackIdx); };
        addAndMakeVisible(btn);
    }

    PanaudiumProcessor& proc;
    int currentRack = 2;

    juce::TextButton subTabA, subTabB, subTabCommon;
    std::vector<std::unique_ptr<FxSlotStrip>> rackStrips[op::R_NumRacks];
};

inline void FxSlotStrip::moveSlot(int delta) {
    rackComp.swapSlots(rackIndex, slotIndex, slotIndex + delta);
}
