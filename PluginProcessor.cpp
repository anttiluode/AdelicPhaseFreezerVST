#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath> // Added for C++17 compatibility

juce::AudioProcessorValueTreeState::ParameterLayout AdelicProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>("beta", "Beta (Cooling)", 0.0f, 2.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eml_drive", "EML Gate Drive", 0.0f, 10.0f, 2.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("time", "Base Time (ms)", 1.0f, 50.0f, 15.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("feedback", "Resonance", 0.0f, 0.98f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("mix", "Dry/Wet", 0.0f, 1.0f, 0.5f));
    return layout;
}

AdelicProcessor::AdelicProcessor()
    : AudioProcessor(BusesProperties().withInput("In", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Out", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout()) {}

// ---> THIS IS THE MISSING PLUMBING <---
juce::AudioProcessorEditor* AdelicProcessor::createEditor() {
    return new AdelicEditor(*this);
}

void AdelicProcessor::prepareToPlay(double sampleRate, int) {
    for (int ch = 0; ch < 2; ++ch) {
        takensDelay[ch].prepare(sampleRate, 50.0); // Max 50ms for Takens embedding
        for (int k = 0; k < NUM_ARMS; ++k) {
            armDelays[ch][k].prepare(sampleRate, 1000.0); // Max 1sec
        }
    }
}

void AdelicProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    float beta     = apvts.getRawParameterValue("beta")->load();
    float drive    = apvts.getRawParameterValue("eml_drive")->load();
    float baseTime = apvts.getRawParameterValue("time")->load();
    float fb       = apvts.getRawParameterValue("feedback")->load();
    float mix      = apvts.getRawParameterValue("mix")->load();

    // The Beta interpolation factor (0.0 = Prime, 1.0 = Harmonic)
    float lockPhase = juce::jlimit(0.0f, 1.0f, beta);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        float* channelData = buffer.getWritePointer(ch);

        for (int s = 0; s < buffer.getNumSamples(); ++s) {
            float x_t = channelData[s];
            
            // 1. TAKENS EMBEDDING
            float x_td = takensDelay[ch].read(15.0f); // 15ms delay
            takensDelay[ch].write(x_t);

            // Send channel 0 to GUI scope
            if (ch == 0 && s % 8 == 0) {
                scopeX.store(x_t);
                scopeY.store(x_td);
            }

            // 2. THE EML GATE: |Z| - arg(Z)
            float mag = std::sqrt(x_t * x_t + x_td * x_td) * 5.0f; 
            float phase = std::abs(std::atan2(x_td, x_t)) / juce::MathConstants<float>::pi;
            
            // Topological Frustration
            float frustration = std::abs(mag - phase);
            
            // Apply topological distortion
            float gate = std::exp(-drive * frustration);
            float x_eml = std::tanh(x_t * gate * (1.0f + drive));

            // 3. BOST-CONNES DELAY ARMS
            float armSum = 0.0f;
            for (int k = 0; k < NUM_ARMS; ++k) {
                // Fixed C++17 compatible manual interpolation: a + t * (b - a)
                float targetLength = (primes[k] + lockPhase * (harmonics[k] - primes[k])) * baseTime;
                armSum += armDelays[ch][k].read(targetLength);
            }
            armSum /= (float)NUM_ARMS;

            // 4. WRITE BACK TO ARMS WITH FEEDBACK
            for (int k = 0; k < NUM_ARMS; ++k) {
                armDelays[ch][k].write(x_eml + armSum * fb);
            }

            // 5. OUTPUT MIX (Manual lerp)
            float wet = std::tanh(armSum);
            channelData[s] = x_t + mix * (wet - x_t);
        }
    }
}

void AdelicProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void AdelicProcessor::setStateInformation(const void* data, int size) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AdelicProcessor(); }