#include "PluginProcessor.h"
#include "PluginEditor.h"

SimpleMBCompAudioProcessor::SimpleMBCompAudioProcessor()
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
    using namespace Params;
    const auto& params = GetParams();
    
    auto floatHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };
    
    floatHelper(compressor.attack, Names::Attack_Low_Band);
    floatHelper(compressor.release, Names::Release_Low_Band);
    floatHelper(compressor.threshold, Names::Threshold_Low_Band);
    
    auto choiceHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };
    
    choiceHelper(compressor.ratio, Names::Ratio_Low_Band);
    
    auto boolHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };
    
    boolHelper(compressor.bypassed, Names::Bypassed_Low_Band);
    
    floatHelper(lowCrossOver, Names::Low_Mid_Crossover_Freq);
    
    LP.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    AP.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
}

SimpleMBCompAudioProcessor::~SimpleMBCompAudioProcessor(){}

const juce::String SimpleMBCompAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleMBCompAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleMBCompAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleMBCompAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleMBCompAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleMBCompAudioProcessor::getNumPrograms()
{
    return 1;
}

int SimpleMBCompAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleMBCompAudioProcessor::setCurrentProgram (int index){}

const juce::String SimpleMBCompAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleMBCompAudioProcessor::changeProgramName (int index, const juce::String& newName){}

void SimpleMBCompAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    
    compressor.prepare(spec);
    
    LP.prepare(spec);
    HP.prepare(spec);
    AP.prepare(spec);
    
    apBuffer.setSize(spec.numChannels, samplesPerBlock);
    
    for(auto& buffer : filterBuffers)
    {
        buffer.setSize(spec.numChannels, samplesPerBlock);
    }
}

void SimpleMBCompAudioProcessor::releaseResources(){}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleMBCompAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
  #endif
}
#endif

void SimpleMBCompAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    //compressor.updateCompressorSettings();
    //compressor.process(buffer);
    
    for(auto& fb : filterBuffers)
    {
        fb = buffer;
    }
    
    auto cutoff = lowCrossOver->get();
    LP.setCutoffFrequency(cutoff);
    HP.setCutoffFrequency(cutoff);
    
    auto fb0Block = juce::dsp::AudioBlock<float>(filterBuffers[0]);
    auto fb1Block = juce::dsp::AudioBlock<float>(filterBuffers[1]);
    
    auto fb0Ctx = juce::dsp::ProcessContextReplacing<float>(fb0Block);
    auto fb1Ctx = juce::dsp::ProcessContextReplacing<float>(fb1Block);
    
    LP.process(fb0Ctx);
    HP.process(fb1Ctx);
    
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();
    
//    if(compressor.bypassed->get())
//        return;
    
    apBuffer = buffer;
    auto apBlock = juce::dsp::AudioBlock<float>(apBuffer);
    auto apContext = juce::dsp::ProcessContextReplacing<float>(apBlock);
    AP.process(apContext);
    
    buffer.clear();
    
    auto addFilterBand = [nc = numChannels, ns = numSamples](auto& inputBuffer, const auto& source)
    {
        for(auto i = 0; i < nc; i++)
        {
            inputBuffer.addFrom(i, 0, source, i, 0, ns);
        }
    };
    
    if(!compressor.bypassed->get())
    {
        addFilterBand(buffer, filterBuffers[0]);
        addFilterBand(buffer, filterBuffers[1]);
    }
    else
    {
        addFilterBand(buffer, apBuffer);
    }

}

bool SimpleMBCompAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SimpleMBCompAudioProcessor::createEditor()
{
    //return new SimpleMBCompAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

void SimpleMBCompAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void SimpleMBCompAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    
    if(tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleMBCompAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    
    using namespace juce;
    using namespace Params;
    
    const auto& params = GetParams();
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Threshold_Low_Band),
                                                     params.at(Names::Threshold_Low_Band),
                                                     NormalisableRange<float>(-60, 12, 1, 1),
                                                     0));
    
    auto attackReleaseRange = NormalisableRange<float>(5, 500, 1, 1);
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Attack_Low_Band),
                                                     params.at(Names::Attack_Low_Band),
                                                     attackReleaseRange,
                                                     50));
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Release_Low_Band),
                                                     params.at(Names::Release_Low_Band),
                                                     attackReleaseRange,
                                                     250));
    
    auto choices = std::vector<double> {1, 1.5, 2, 3, 4, 5, 6, 7, 8, 10, 15, 20, 50, 100};
    StringArray sa;
    
    for(auto choice : choices)
    {
        sa.add(juce::String(choice,1));
    }
    
    layout.add(std::make_unique<AudioParameterChoice>(params.at(Names::Ratio_Low_Band),
                                                      params.at(Names::Ratio_Low_Band),
                                                      sa,
                                                      3));
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Bypassed_Low_Band),
                                                    params.at(Names::Bypassed_Low_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Low_Mid_Crossover_Freq),
                                                     params.at(Names::Low_Mid_Crossover_Freq),
                                                     NormalisableRange<float>(20, 20000, 1, 1),
                                                     500));
    
    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleMBCompAudioProcessor();
}
