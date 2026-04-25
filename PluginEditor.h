#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <deque>

class AdelicEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit AdelicEditor(AdelicProcessor&);
    ~AdelicEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    AdelicProcessor& processor;

    juce::Slider sBeta, sDrive, sTime, sFb, sMix;
    juce::Label  lBeta, lDrive, lTime, lFb, lMix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aBeta, aDrive, aTime, aFb, aMix;

    // Phase Space Plotter Data
    std::deque<juce::Point<float>> scopePath;
    static constexpr int MAX_SCOPE_PTS = 200;
    
    // Auto-zoom tracking
    float currentScale = 1.0f;

    void setupSlider(juce::Slider& s, juce::Label& l, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdelicEditor)
};