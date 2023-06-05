/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FlangerPluginAudioProcessor::FlangerPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    addParameter(mDryWetParameter = new juce::AudioParameterFloat("drywet",
        "Dry Wet",
        0.0,
        1.0,
        0.5));

    addParameter(mFeedbackParameter = new juce::AudioParameterFloat("feedback",
        "Feedback",
        0,
        0.98,
        0.5));

    addParameter(mRateParameter = new juce::AudioParameterFloat("rate",
        "Rate",
        0.1f,
        20.f,
        10.f));

    addParameter(mPhaseOffsetParameter = new juce::AudioParameterFloat("phaseoffset",
        "Phase Offset",
        0.0f,
        1.f,
        0.f));

 

    addParameter(mDepthParameter = new juce::AudioParameterFloat("depth",
        "Depth",
        0.0,
        1.0,
        0.5));


    addParameter(mTypeParameter = new juce::AudioParameterInt("type",
        "Type",
        0,
        1,
        0));

    mCircularBufferLeft = nullptr;
    mCircularBufferRight = nullptr;
    mCircularBufferWriteHead = 0;
    mCirqularBufferLength = 0;
   // mDelayTimeInSamples = 0;
   // mDelayReadHead = 0;

    mFeedbackLeft = 0;
    mFeedbackRight = 0;

    mLFOPhase = 0;
}

FlangerPluginAudioProcessor::~FlangerPluginAudioProcessor()
{
}

//==============================================================================
const juce::String FlangerPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FlangerPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FlangerPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FlangerPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FlangerPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FlangerPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int FlangerPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FlangerPluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String FlangerPluginAudioProcessor::getProgramName (int index)
{
    return {};
}

void FlangerPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void FlangerPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mLFOPhase = 0;

    mCirqularBufferLength = sampleRate * MAX_DELAY_TIME;


    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    if (mCircularBufferLeft == nullptr) {
        mCircularBufferLeft = new float[mCirqularBufferLength];
    }
    juce::zeromem(mCircularBufferLeft, mCirqularBufferLength * sizeof(float));

    if (mCircularBufferRight == nullptr) {
        mCircularBufferRight = new float[mCirqularBufferLength];



    }

    juce::zeromem(mCircularBufferRight, mCirqularBufferLength * sizeof(float));

    mCircularBufferWriteHead = 0;

    

}

void FlangerPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FlangerPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void FlangerPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    DBG("DRY WET: " << * mDryWetParameter);
    DBG("DEPTH: " << *mDepthParameter);
    DBG("RATE: " << *mRateParameter);
    DBG("PHASE: " << *mPhaseOffsetParameter);
    DBG("FEEDBACK: " << *mFeedbackParameter);
    DBG("TYPE: " << (int)*mTypeParameter);

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());



    float* leftChanel = buffer.getWritePointer(0);
    float* rightChanel = buffer.getWritePointer(1);


    for (int i = 0; i < buffer.getNumSamples(); i++) {

        float lfoOutLeft = std::sin(2 * juce::MathConstants<double>::pi * mLFOPhase);
       
        float lfoPhaseRight = mLFOPhase + *mPhaseOffsetParameter;
        if (lfoPhaseRight > 1) {
            lfoPhaseRight -= 1;
        }

        float lfoOutRight = std::sin(2 * juce::MathConstants<double>::pi * lfoPhaseRight);




        lfoOutLeft *= *mDepthParameter;
        lfoOutRight *= *mDepthParameter;

        float lfoOutMappedLeft = 0;
        float lfoOutMappedRight = 0;
        
        //chorus effect
        if (*mTypeParameter == 0) {
             lfoOutMappedLeft = juce::jmap(lfoOutLeft, -1.f, 1.f, 0.005f, 0.03f);
             lfoOutMappedRight = juce::jmap(lfoOutRight, -1.f, 1.f, 0.005f, 0.030f);

            //flanger effect
        }
        else {
             lfoOutMappedLeft = juce::jmap(lfoOutLeft, -1.f, 1.f, 0.001f, 0.005f);
             lfoOutMappedRight = juce::jmap(lfoOutRight, -1.f, 1.f, 0.001f, 0.005f);
        }


        float delayTimeSamplesLeft = getSampleRate() * lfoOutMappedLeft;

        
        float delayTimeSamplesRight = getSampleRate() * lfoOutMappedRight;


        mLFOPhase += *mRateParameter / getSampleRate();

        if (mLFOPhase > 1) {
            mLFOPhase -= 1;
        }

      

        mCircularBufferLeft[mCircularBufferWriteHead] = leftChanel[i] + mFeedbackLeft;
        mCircularBufferRight[mCircularBufferWriteHead] = rightChanel[i] + mFeedbackRight;

        float delayReadHeadLeft = mCircularBufferWriteHead - delayTimeSamplesLeft;

        if (delayReadHeadLeft < 0) {
            delayReadHeadLeft += mCirqularBufferLength;
        }

     

        float delayReadHeadRight = mCircularBufferWriteHead - delayTimeSamplesRight;


        if (delayReadHeadRight < 0) {
            delayReadHeadRight += mCirqularBufferLength;
        }


        int readHeadLeft_x = (int)delayReadHeadLeft;
        int readHeadLeft_x1 = readHeadLeft_x + 1;
        float readHeadFloatLeft = delayReadHeadLeft - readHeadLeft_x;


        if (readHeadLeft_x1 >= mCirqularBufferLength) {
            readHeadLeft_x1 -= mCirqularBufferLength;
        }

        int readHeadRight_x = (int)delayReadHeadRight;
        int readHeadRight_x1 = readHeadRight_x + 1;
        float readHeadFloatRight = delayReadHeadRight - readHeadRight_x;


        if (readHeadRight_x1 >= mCirqularBufferLength) {
            readHeadRight_x1 -= mCirqularBufferLength;
        }



        float  delay_sample_left = lin_interp(mCircularBufferLeft[readHeadLeft_x], mCircularBufferLeft[readHeadLeft_x1], readHeadFloatLeft);
        float   delay_sample_right = lin_interp(mCircularBufferRight[readHeadRight_x], mCircularBufferRight[readHeadRight_x1], readHeadFloatRight);

        mFeedbackLeft = delay_sample_left * *mFeedbackParameter;
        mFeedbackRight = delay_sample_right * *mFeedbackParameter;


        mCircularBufferWriteHead++;

        buffer.addSample(0, i, buffer.getSample(0, i) * (1 - *mDryWetParameter) + delay_sample_left * *mDryWetParameter);
        buffer.addSample(1, i, buffer.getSample(1, i) * (1 - *mDryWetParameter) + delay_sample_right * *mDryWetParameter);

        if (mCircularBufferWriteHead >= mCirqularBufferLength) {
            mCircularBufferWriteHead = 0;
        }




    }
}

//==============================================================================
bool FlangerPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* FlangerPluginAudioProcessor::createEditor()
{
    return new FlangerPluginAudioProcessorEditor (*this);
}

//==============================================================================
void FlangerPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{

    std::unique_ptr <juce::XmlElement> xml(new juce::XmlElement("FlangerChorus"));

    xml->setAttribute("DryWet", *mDryWetParameter);
    xml->setAttribute("Depth", *mDepthParameter);
    xml->setAttribute("Rate", *mRateParameter);
    xml->setAttribute("PhaseOffset", *mPhaseOffsetParameter);
    xml->setAttribute("Feedback", *mFeedbackParameter);
    xml->setAttribute("Type", *mTypeParameter);

    copyXmlToBinary(*xml, destData);




    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}



void FlangerPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{

    std::unique_ptr <juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml.get() != nullptr && xml->hasTagName("FlangerChorus")) {


        *mDryWetParameter = xml->getDoubleAttribute("DryWet");
        *mDepthParameter = xml->getDoubleAttribute("Depth");
        *mRateParameter = xml->getDoubleAttribute("Rate");
        *mPhaseOffsetParameter = xml->getDoubleAttribute("PhaseOffset");
        *mFeedbackParameter = xml->getDoubleAttribute("FeedBack");
        *mTypeParameter = xml->getIntAttribute("Type");
    }

    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    }

    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlangerPluginAudioProcessor();
}
float FlangerPluginAudioProcessor::lin_interp(float sample_x, float sample_x1, float inPhase)
{
    return (1 - inPhase) * sample_x + inPhase * sample_x1;
}


