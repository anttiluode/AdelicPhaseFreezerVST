#include "PluginEditor.h"

static const juce::Colour BG { 0xff0a0a12 };
static const juce::Colour TEAL { 0xff44ffaa };
static const juce::Colour PURPLE { 0xffcc66ff };

AdelicEditor::AdelicEditor(AdelicProcessor& p) : AudioProcessorEditor(&p), processor(p) {
    // Make the default size larger
    setSize(800, 600);
    
    // ALLOW FREESTYLE RESIZING
    setResizable(true, true);
    setResizeLimits(500, 400, 2500, 2000); // Min and Max window size

    setupSlider(sBeta, lBeta, "Cooling (Beta)");
    setupSlider(sDrive, lDrive, "EML Frustration");
    setupSlider(sTime, lTime, "Base Delay (ms)");
    setupSlider(sFb, lFb, "Resonance");
    setupSlider(sMix, lMix, "Mix");

    aBeta = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "beta", sBeta);
    aDrive = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "eml_drive", sDrive);
    aTime = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "time", sTime);
    aFb = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "feedback", sFb);
    aMix = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "mix", sMix);

    startTimerHz(45); // 45 FPS UI updates
}

AdelicEditor::~AdelicEditor() { stopTimer(); }

void AdelicEditor::setupSlider(juce::Slider& s, juce::Label& l, const juce::String& name) {
    addAndMakeVisible(s);
    addAndMakeVisible(l);
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    s.setColour(juce::Slider::thumbColourId, TEAL);
    s.setColour(juce::Slider::rotarySliderFillColourId, PURPLE);
    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, juce::Colours::grey);
}

void AdelicEditor::timerCallback() {
    float x = processor.scopeX.load();
    float y = processor.scopeY.load();
    scopePath.push_back({x, y});
    if (scopePath.size() > MAX_SCOPE_PTS) scopePath.pop_front();
    repaint();
}

void AdelicEditor::paint(juce::Graphics& g) {
    g.fillAll(BG);

    // Dynamic Scope Area (leaves exactly 130px at the bottom for the knobs)
    juce::Rectangle<int> scopeRect(20, 20, getWidth() - 40, getHeight() - 150);
    g.setColour(juce::Colour(0xff111420));
    g.fillRect(scopeRect);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRect(scopeRect);

    // Draw Crosshairs
    int cx = scopeRect.getCentreX();
    int cy = scopeRect.getCentreY();
    g.drawLine(cx, scopeRect.getY(), cx, scopeRect.getBottom());
    g.drawLine(scopeRect.getX(), cy, scopeRect.getRight(), cy);

    if (scopePath.size() > 1) {
        // --- AUTO-ZOOM ALGORITHM ---
        float maxAbs = 0.001f;
        for (const auto& pt : scopePath) {
            maxAbs = std::max({maxAbs, std::abs(pt.x), std::abs(pt.y)});
        }

        // Calculate target scale to always fill 45% of the shortest dimension
        float minDim = std::min(scopeRect.getWidth(), scopeRect.getHeight());
        float targetScale = (minDim * 0.45f) / maxAbs;

        // Smoothly interpolate the scale (Fast zoom-out, slow zoom-in)
        if (targetScale < currentScale) {
            currentScale = currentScale * 0.8f + targetScale * 0.2f; // Quick response to loud peaks
        } else {
            currentScale = currentScale * 0.98f + targetScale * 0.02f; // Smooth glide for quiet tails
        }

        juce::Path p;
        for (size_t i = 0; i < scopePath.size(); ++i) {
            float px = cx + scopePath[i].x * currentScale;
            float py = cy - scopePath[i].y * currentScale; // Invert Y
            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }
        
        // Color shifts from Ergodic (Purple) to Locked (Teal) based on Beta
        float beta = sBeta.getValue();
        juce::Colour traceColor = PURPLE.interpolatedWith(TEAL, juce::jlimit(0.0f, 1.0f, beta));
        
        // Glow effect
        g.setColour(traceColor.withAlpha(0.2f));
        g.strokePath(p, juce::PathStrokeType(4.0f)); 
        
        // Main trace
        g.setColour(traceColor.withAlpha(0.9f));
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
}

void AdelicEditor::resized() {
    int w = 80;
    // Anchor knobs to the bottom dynamically
    int y = getHeight() - 110; 
    int spacing = (getWidth() - (w * 5)) / 6;

    int curX = spacing;
    auto positionKnob = [&](juce::Slider& s, juce::Label& l) {
        s.setBounds(curX, y + 20, w, w);
        l.setBounds(curX, y, w, 20);
        curX += w + spacing;
    };

    positionKnob(sBeta, lBeta);
    positionKnob(sDrive, lDrive);
    positionKnob(sTime, lTime);
    positionKnob(sFb, lFb);
    positionKnob(sMix, lMix);
}