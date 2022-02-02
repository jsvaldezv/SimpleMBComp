#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SimpleMBCompAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    
    SimpleMBCompAudioProcessorEditor (SimpleMBCompAudioProcessor&);
    ~SimpleMBCompAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:

    SimpleMBCompAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleMBCompAudioProcessorEditor)
};
