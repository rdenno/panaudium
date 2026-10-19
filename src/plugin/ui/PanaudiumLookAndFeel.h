#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PanaudiumLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PanaudiumLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff121418));

        // Slider colours
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff222733));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe5a83b));
        setColour(juce::Slider::thumbColourId, juce::Colours::white);
        setColour(juce::Slider::trackColourId, juce::Colour(0xff181b22));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffcbd5e1));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00000000));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
        setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0x40e5a83b));

        // ComboBox colours
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff181b22));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2c3240));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
        setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff1e222b));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff94a3b8));
        setColour(juce::ComboBox::focusedOutlineColourId, juce::Colour(0xff4a5568));

        // PopupMenu colours
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff181b22));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe2e8f0));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff2c3444));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        // TextButton colours
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c202a));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2b3342));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        // Label colours
        setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                          juce::Slider& slider) override {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        if (radius <= 2.0f) return;

        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float lineW = juce::jlimit(2.5f, 4.2f, radius * 0.16f);
        const float arcRadius = radius - lineW * 0.5f - 1.0f;
        const bool enabled = slider.isEnabled();

        // 1. Background arc
        juce::Path bgArc;
        bgArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(enabled ? slider.findColour(juce::Slider::rotarySliderOutlineColourId)
                            : juce::Colour(0xff181a20));
        g.strokePath(bgArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. Value arc
        const float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        if (enabled && std::abs(toAngle - rotaryStartAngle) > 0.01f) {
            juce::Path valArc;
            valArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
            const juce::Colour fillCol = slider.findColour(juce::Slider::rotarySliderFillColourId);
            g.setColour(fillCol);
            g.strokePath(valArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Knob Body
        const float knobRadius = arcRadius - lineW * 0.5f - 2.5f;
        if (knobRadius > 3.0f) {
            // Knob body gradient
            juce::Colour topCol = enabled ? juce::Colour(0xff272c38) : juce::Colour(0xff1c1f26);
            juce::Colour botCol = enabled ? juce::Colour(0xff14171d) : juce::Colour(0xff121417);
            g.setGradientFill(juce::ColourGradient(topCol, cx, cy - knobRadius, botCol, cx, cy + knobRadius, false));
            g.fillEllipse(cx - knobRadius, cy - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

            // Metallic rim
            g.setColour(enabled ? juce::Colour(0xff3b4354) : juce::Colour(0xff222630));
            g.drawEllipse(cx - knobRadius, cy - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.2f);

            // 4. Pointer line
            const float angle = toAngle - juce::MathConstants<float>::halfPi;
            const float rInner = knobRadius * 0.35f;
            const float rOuter = knobRadius * 0.88f;
            const float px1 = cx + rInner * std::cos(angle);
            const float py1 = cy + rInner * std::sin(angle);
            const float px2 = cx + rOuter * std::cos(angle);
            const float py2 = cy + rOuter * std::sin(angle);

            g.setColour(enabled ? juce::Colours::white : juce::Colour(0xff4a5260));
            g.drawLine(px1, py1, px2, py2, juce::jlimit(1.8f, 2.5f, radius * 0.1f));
        }
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override {
        if (style == juce::Slider::LinearHorizontal) {
            const float trackH = 4.0f;
            const float trackY = (float)y + (float)height * 0.5f - trackH * 0.5f;
            const auto trackRect = juce::Rectangle<float>((float)x, trackY, (float)width, trackH);

            // Track background
            g.setColour(juce::Colour(0xff181a20));
            g.fillRoundedRectangle(trackRect, 2.0f);
            g.setColour(juce::Colour(0xff2a2e38));
            g.drawRoundedRectangle(trackRect, 2.0f, 1.0f);

            // Active track fill
            const float activeW = juce::jlimit(0.0f, (float)width, sliderPos - (float)x);
            if (activeW > 1.0f) {
                const auto activeRect = juce::Rectangle<float>((float)x, trackY, activeW, trackH);
                g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
                g.fillRoundedRectangle(activeRect, 2.0f);
            }

            // Thumb
            const float thumbW = 8.0f;
            const float thumbH = 14.0f;
            const auto thumbRect = juce::Rectangle<float>(sliderPos - thumbW * 0.5f,
                                                          (float)y + (float)height * 0.5f - thumbH * 0.5f,
                                                          thumbW, thumbH);
            g.setColour(juce::Colour(0xff272c38));
            g.fillRoundedRectangle(thumbRect, 2.0f);
            g.setColour(juce::Colour(0xffcbd5e1));
            g.drawRoundedRectangle(thumbRect, 2.0f, 1.2f);
        } else {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool isMouseButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override {
        const auto r = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
        const float corner = 4.0f;

        // Background
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(r, corner);

        // Border
        juce::Colour borderCol = box.findColour(juce::ComboBox::outlineColourId);
        if (box.hasKeyboardFocus(true) || isMouseButtonDown) {
            borderCol = box.findColour(juce::ComboBox::focusedOutlineColourId);
        }
        g.setColour(borderCol);
        g.drawRoundedRectangle(r, corner, 1.0f);

        // Chevron Arrow
        juce::Path arrow;
        const float ax = (float)buttonX + (float)buttonW * 0.5f;
        const float ay = (float)buttonY + (float)buttonH * 0.5f;
        arrow.startNewSubPath(ax - 3.5f, ay - 2.0f);
        arrow.lineTo(ax, ay + 2.5f);
        arrow.lineTo(ax + 3.5f, ay - 2.0f);

        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.strokePath(arrow, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override {
        return juce::Font(juce::FontOptions(12.0f));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const float corner = 4.0f;
        const bool toggled = button.getToggleState();

        juce::Colour bg = backgroundColour;
        if (toggled) {
            bg = button.findColour(juce::TextButton::buttonOnColourId);
        }
        if (shouldDrawButtonAsDown) {
            bg = bg.darker(0.15f);
        } else if (shouldDrawButtonAsHighlighted) {
            bg = bg.brighter(0.12f);
        }

        g.setColour(bg);
        g.fillRoundedRectangle(bounds, corner);

        // Outline / active glow
        if (toggled) {
            g.setColour(juce::Colour(0xffe5a83b)); // gold highlight for active tab
            g.drawRoundedRectangle(bounds, corner, 1.2f);
            // Glowing bottom line
            g.fillRect(bounds.getX() + 4.0f, bounds.getBottom() - 2.5f, bounds.getWidth() - 8.0f, 2.0f);
        } else {
            g.setColour(juce::Colour(0xff2c3240));
            g.drawRoundedRectangle(bounds, corner, 1.0f);
        }
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
        return juce::Font(juce::FontOptions(juce::jlimit(11.0f, 13.0f, (float)buttonHeight * 0.45f), juce::Font::bold));
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool) override {
        const auto bounds = button.getLocalBounds().toFloat();
        const bool on = button.getToggleState();

        if (button.getButtonText().isEmpty()) {
            // Power LED style toggle
            const float size = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 4.0f;
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            const float r = size * 0.5f;

            // Outer bezel ring
            g.setColour(juce::Colour(0xff252932));
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour(juce::Colour(0xff3a4150));
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);

            const float ledR = r - 2.5f;
            if (on) {
                // Glowing neon green / amber LED
                const juce::Colour ledCol(0xff38ef7d);
                g.setColour(ledCol.withAlpha(0.3f));
                g.fillEllipse(cx - (ledR + 2.0f), cy - (ledR + 2.0f), (ledR + 2.0f) * 2.0f, (ledR + 2.0f) * 2.0f);
                g.setColour(ledCol);
                g.fillEllipse(cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f);
                // Highlight glint
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.fillEllipse(cx - ledR * 0.5f, cy - ledR * 0.6f, ledR * 0.6f, ledR * 0.4f);
            } else {
                // Dark sunken unlit LED
                g.setColour(juce::Colour(0xff14171d));
                g.fillEllipse(cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f);
                g.setColour(juce::Colour(0xff222630));
                g.drawEllipse(cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f, 0.8f);
            }
        } else {
            // Standard toggle with text
            const float tickSize = 14.0f;
            const auto tickBounds = juce::Rectangle<float>(bounds.getX() + 2.0f, bounds.getCentreY() - tickSize * 0.5f, tickSize, tickSize);

            g.setColour(juce::Colour(0xff181b22));
            g.fillRoundedRectangle(tickBounds, 3.0f);
            g.setColour(on ? juce::Colour(0xffe5a83b) : juce::Colour(0xff2c3240));
            g.drawRoundedRectangle(tickBounds, 3.0f, 1.0f);

            if (on) {
                g.setColour(juce::Colour(0xffe5a83b));
                g.fillRoundedRectangle(tickBounds.reduced(3.0f), 2.0f);
            }

            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.setColour(on ? juce::Colours::white : (shouldDrawButtonAsHighlighted ? juce::Colour(0xffcbd5e1) : juce::Colour(0xff94a3b8)));
            g.drawFittedText(button.getButtonText(),
                             (int)(tickBounds.getRight() + 6.0f), 0,
                             (int)(bounds.getWidth() - tickBounds.getRight() - 6.0f), (int)bounds.getHeight(),
                             juce::Justification::centredLeft, 1);
        }
    }

    juce::Font getPopupMenuFont() override {
        return juce::Font(juce::FontOptions(12.5f));
    }

    juce::Font getLabelFont(juce::Label&) override {
        return juce::Font(juce::FontOptions(11.0f));
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool,
                           const juce::String& text, const juce::String&,
                           const juce::Drawable*, const juce::Colour*) override {
        if (isSeparator) {
            auto r = area.reduced(5, 0);
            g.setColour(juce::Colour(0xff2a303d));
            g.fillRect(r.removeFromTop(1));
            return;
        }

        if (isHighlighted && isActive) {
            g.setColour(juce::Colour(0xff283142));
            g.fillRoundedRectangle(area.toFloat().reduced(2.0f, 1.0f), 3.0f);
        }

        g.setFont(getPopupMenuFont());
        g.setColour(isActive ? (isHighlighted ? juce::Colours::white : juce::Colour(0xffe2e8f0))
                             : juce::Colour(0xff555c6b));

        auto textArea = area.reduced(10, 0);
        if (isTicked) {
            g.setColour(juce::Colour(0xffe5a83b));
            g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x93"), textArea.removeFromLeft(14), juce::Justification::centredLeft, true);
        }

        g.setColour(isActive ? (isHighlighted ? juce::Colours::white : juce::Colour(0xffe2e8f0))
                             : juce::Colour(0xff555c6b));
        g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);
    }
};
