#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

class EnvelopeView : public juce::Component {
public:
    EnvelopeView(bool isFilter = false) : filterEnv(isFilter) {}

    void setADSR(float a, float d, float s, float r) {
        if (std::abs(attack - a) > 0.0001f || std::abs(decay - d) > 0.0001f ||
            std::abs(sustain - s) > 0.0001f || std::abs(release - r) > 0.0001f) {
            attack = a;
            decay = d;
            sustain = s;
            release = r;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(2.0f);
        // Sunken dark display
        g.setColour(juce::Colour(0xff0d0f14));
        g.fillRoundedRectangle(b, 4.0f);
        g.setColour(juce::Colour(0xff222733));
        g.drawRoundedRectangle(b, 4.0f, 1.0f);

        // Subtle horizontal midline
        g.setColour(juce::Colour(0x15ffffff));
        g.drawHorizontalLine(juce::roundToInt(b.getY() + b.getHeight() * 0.5f), b.getX() + 4.0f, b.getRight() - 4.0f);

        // Map times 0.001..20 s non-linearly to 0..1 so short times are visible
        auto mapTime = [](float t) {
            return std::log10(juce::jlimit(0.001f, 20.0f, t) / 0.001f + 1.0f) / std::log10(20.0f / 0.001f + 1.0f);
        };

        const float an = mapTime(attack);
        const float dn = mapTime(decay);
        const float rn = mapTime(release);
        const float sn = juce::jlimit(0.0f, 1.0f, sustain);

        const float totalW = an + dn + 0.35f + rn;
        const float w = b.getWidth() - 16.0f;
        const float h = b.getHeight() - 14.0f;
        const float startX = b.getX() + 8.0f;
        const float bottomY = b.getY() + 7.0f + h;
        const float topY = b.getY() + 7.0f;

        const float aW = (an / totalW) * w;
        const float dW = (dn / totalW) * w;
        const float sW = (0.35f / totalW) * w;
        const float rW = (rn / totalW) * w;

        const float x0 = startX;
        const float y0 = bottomY;
        const float x1 = x0 + aW;
        const float y1 = topY;
        const float x2 = x1 + dW;
        const float y2 = bottomY - sn * h;
        const float x3 = x2 + sW;
        const float y3 = y2;
        const float x4 = x3 + rW;
        const float y4 = bottomY;

        juce::Path p;
        p.startNewSubPath(x0, y0);
        p.lineTo(x1, y1);
        p.lineTo(x2, y2);
        p.lineTo(x3, y3);
        p.lineTo(x4, y4);

        const juce::Colour accent = filterEnv ? juce::Colour(0xff38bdf8) : juce::Colour(0xfffb923c);

        juce::Path fillP = p;
        fillP.lineTo(x4, bottomY);
        fillP.lineTo(x0, bottomY);
        fillP.closeSubPath();

        g.setGradientFill(juce::ColourGradient(accent.withAlpha(0.35f), 0.0f, topY,
                                               accent.withAlpha(0.02f), 0.0f, bottomY, false));
        g.fillPath(fillP);

        g.setColour(accent);
        g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Vertex dots
        g.setColour(juce::Colours::white);
        g.fillEllipse(x1 - 2.0f, y1 - 2.0f, 4.0f, 4.0f);
        g.fillEllipse(x2 - 2.0f, y2 - 2.0f, 4.0f, 4.0f);
        g.fillEllipse(x3 - 2.0f, y3 - 2.0f, 4.0f, 4.0f);
    }

private:
    float attack = 0.005f, decay = 0.5f, sustain = 0.8f, release = 0.4f;
    bool filterEnv = false;
};
