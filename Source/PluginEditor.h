/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class FlangerPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    FlangerPluginAudioProcessorEditor (FlangerPluginAudioProcessor&);
    ~FlangerPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:

    juce::Slider mDryWetSlider;
    juce::Slider mDepthSlider;
    juce::Slider mRateSlider;
    juce::Slider mPhaseOffsetSlider;
    juce::Slider mFeedbackSlider;
    
    juce::ComboBox mType;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FlangerPluginAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangerPluginAudioProcessorEditor)
};
