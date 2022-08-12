/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <vector>

//==============================================================================
NewProjectAudioProcessor::NewProjectAudioProcessor()
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
    using namespace juce;
    using namespace Params;
    const auto& params = GetParams();
    auto floatHelper = [&apvts = this->apvts, &params ](auto& Param,
        const auto& ParamName ) {
        Param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(params.at(ParamName)));
        jassert(Param);
    };
    floatHelper(lowCompBand.Attack, Names::Attack_Low_Band);
    floatHelper(lowCompBand.Release, Names::Release_Low_Band);
    floatHelper(lowCompBand.Threshold, Names::Threshold_Low_Band);

    floatHelper(midCompBand.Attack, Names::Attack_Mid_Band);
    floatHelper(midCompBand.Release, Names::Release_Mid_Band);
    floatHelper(midCompBand.Threshold, Names::Threshold_Mid_Band);

    floatHelper(highCompBand.Attack, Names::Attack_High_Band);
    floatHelper(highCompBand.Release, Names::Release_High_Band);
    floatHelper(highCompBand.Threshold, Names::Threshold_High_Band);
    floatHelper(inputGain, Names::Input_Gain);
    floatHelper(outputGain, Names::Output_Gain);

    auto choiceHelper = [&apvts = this->apvts, &params](auto& Param,
        const auto& ParamName) {
        Param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params.at(ParamName)));
        jassert(Param);
    };
    choiceHelper(lowCompBand.ratio, Names::Ratio_Low_Band);
    choiceHelper(midCompBand.ratio, Names::Ratio_Mid_Band);
    choiceHelper(highCompBand.ratio, Names::Ratio_High_Band);

    auto boolHelper = [&apvts = this->apvts, &params](auto& Param,
        const auto& ParamName) {
        Param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(params.at(ParamName)));
        jassert(Param);
    };
    boolHelper(lowCompBand.Bypassed, Names::Bypassed_Low_Band);
    boolHelper(midCompBand.Bypassed, Names::Bypassed_Mid_Band);
    boolHelper(highCompBand.Bypassed, Names::Bypassed_High_Band);

    boolHelper(lowCompBand.Mute, Names::Mute_Low_Band);
    boolHelper(midCompBand.Mute, Names::Mute_Mid_Band);
    boolHelper(highCompBand.Mute, Names::Mute_High_Band);

    boolHelper(lowCompBand.Solo, Names::Solo_Low_Band);
    boolHelper(midCompBand.Solo, Names::Solo_Mid_Band);
    boolHelper(highCompBand.Solo, Names::Solo_High_Band);

    floatHelper(lowMidCrossover, Names::Low_Mid_Crossover_Freq);
    floatHelper(midHighCrossover, Names::Mid_High_Crossover_Freq);

    LP1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    AP2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    LP2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    
}

NewProjectAudioProcessor::~NewProjectAudioProcessor()
{
}

//==============================================================================
const juce::String NewProjectAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NewProjectAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NewProjectAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NewProjectAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int NewProjectAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NewProjectAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String NewProjectAudioProcessor::getProgramName (int index)
{
    return {};
}

void NewProjectAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void NewProjectAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    spec.sampleRate = sampleRate;

    LP1.prepare(spec);
    HP1.prepare(spec);
    LP2.prepare(spec);
    HP2.prepare(spec);
    AP2.prepare(spec);
    inGain.prepare(spec);
    outGain.prepare(spec);
    inGain.setRampDurationSeconds(0.05);//50ms
    outGain.setRampDurationSeconds(0.05);

    for (auto& fb : FilterBuffer)
        fb.setSize(spec.numChannels, samplesPerBlock);

    for (auto& compressor : compressors)
        compressor.Prepare(spec);
}

void NewProjectAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NewProjectAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void NewProjectAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels(); 
    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());


    auto lowMidCutoff = lowMidCrossover->get();
    auto midHighCutoff = midHighCrossover->get();

    LP1.setCutoffFrequency(lowMidCutoff);
    HP1.setCutoffFrequency(lowMidCutoff);

    LP2.setCutoffFrequency(midHighCutoff);
    HP2.setCutoffFrequency(midHighCutoff);
    AP2.setCutoffFrequency(midHighCutoff);

  

    for (auto& compressor : compressors)
        compressor.UpdateCompressorSettings();

    inGain.setGainDecibels(inputGain->get());
    outGain.setGainDecibels(outputGain->get());
    applyGain(buffer, inGain);

    for (auto& fb : FilterBuffer)
        fb = buffer;

    auto& fbBlock0 = juce::dsp::AudioBlock<float>(FilterBuffer[0]);
    auto& fbBlock1 = juce::dsp::AudioBlock<float>(FilterBuffer[1]);
    auto& fbBlock2 = juce::dsp::AudioBlock<float>(FilterBuffer[2]);

    auto fbcxt0 = juce::dsp::ProcessContextReplacing<float>(fbBlock0);
    auto fbcxt1 = juce::dsp::ProcessContextReplacing<float>(fbBlock1);
    auto fbcxt2 = juce::dsp::ProcessContextReplacing<float>(fbBlock2);

    LP1.process(fbcxt0);
    AP2.process(fbcxt0);

    HP1.process(fbcxt1);
    FilterBuffer[2] = FilterBuffer[1];
    LP2.process(fbcxt1);
    
    HP2.process(fbcxt2);

    for (size_t i{ 0 }; i < FilterBuffer.size(); i++)
        compressors[i].Process(FilterBuffer[i]);


    auto numChannels = buffer.getNumChannels();
    auto numSamples = buffer.getNumSamples();

   
   
    buffer.clear();

   

    auto addFilterBand = [nc = numChannels, ns = numSamples ](auto& inputBuffer, const auto& source) {
        for (auto i{ 0 }; i < nc; ++i)
            inputBuffer.addFrom(i, 0, source,i,0, ns);
    };

    auto bandIsSoloed = false;
    for (auto& comp : compressors) {
        if (comp.Solo->get()) {
            bandIsSoloed = true;
            break;
        }
    }

    if (bandIsSoloed) {
        for (size_t i{ 0 }; i < FilterBuffer.size(); i++) {
            auto& comp = compressors[i];
            if (comp.Solo->get()) {
                addFilterBand(buffer, FilterBuffer[i]);
            }
        }
    }
    else {
        for (size_t i{ 0 }; i < FilterBuffer.size(); i++) {
            auto& comp = compressors[i];
            if (!comp.Mute->get())
                addFilterBand(buffer, FilterBuffer[i]);
        }
    }

    applyGain(buffer, outGain);
    

   /* addFilterBand(buffer, FilterBuffer[0]);
    addFilterBand(buffer, FilterBuffer[1]);
    addFilterBand(buffer, FilterBuffer[2]); */

}

//==============================================================================
bool NewProjectAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* NewProjectAudioProcessor::createEditor()
{
    return new NewProjectAudioProcessorEditor (*this);
}

//==============================================================================
void NewProjectAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void NewProjectAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        apvts.replaceState(tree);
    }
}

NewProjectAudioProcessor::APVTS::ParameterLayout NewProjectAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout Layout;
    using namespace juce;
    using namespace Params;
    const auto& params = GetParams();

    auto GainRange = NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f);

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Input_Gain),
        params.at(Input_Gain),
       GainRange));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Output_Gain),
        params.at(Output_Gain),
        GainRange));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Threshold_Low_Band),
        params.at(Threshold_Low_Band),
        NormalisableRange<float>(-60, 12, 1, 1), 0));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Threshold_Mid_Band),
        params.at(Threshold_Mid_Band),
        NormalisableRange<float>(-60, 12, 1, 1), 0));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Threshold_High_Band),
        params.at(Threshold_High_Band),
        NormalisableRange<float>(-60, 12, 1, 1), 0));

    auto AttackReleaseRange = NormalisableRange<float>(5, 500, 1, 1);

    Layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Bypassed_Low_Band),
        params.at(Bypassed_Low_Band), false));

    Layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Bypassed_Mid_Band),
        params.at(Bypassed_Mid_Band), false));

    Layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Bypassed_High_Band),
        params.at(Bypassed_High_Band), false));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Attack_Low_Band),
        params.at(Attack_Low_Band), AttackReleaseRange, 50));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Attack_Mid_Band),
        params.at(Attack_Mid_Band), AttackReleaseRange, 50));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Attack_High_Band),
        params.at(Attack_High_Band), AttackReleaseRange, 50));
    
    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Release_Low_Band),
        params.at(Release_Low_Band), AttackReleaseRange, 250));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Release_Mid_Band),
        params.at(Release_Mid_Band), AttackReleaseRange, 250));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Release_High_Band),
        params.at(Release_High_Band), AttackReleaseRange, 250));

    auto choices = std::vector<double>{ 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 10, 15, 20, 50, 100 };

    StringArray sa;
    for (auto choice : choices)
        sa.add(juce::String(choice, 1));

    Layout.add(std::make_unique<AudioParameterChoice>(params.at(Ratio_Low_Band),
        params.at(Ratio_Low_Band), sa, 3));

    Layout.add(std::make_unique<AudioParameterChoice>(params.at(Ratio_Low_Band),
        params.at(Ratio_Mid_Band), sa, 3));

    Layout.add(std::make_unique<AudioParameterChoice>(params.at(Ratio_Low_Band),
        params.at(Ratio_High_Band), sa, 3));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Low_Mid_Crossover_Freq),
        params.at(Low_Mid_Crossover_Freq),
        NormalisableRange<float>(20, 999, 1, 1), 400));

    Layout.add(std::make_unique<AudioParameterFloat>(params.at(Mid_High_Crossover_Freq),
        params.at(Mid_High_Crossover_Freq),
        NormalisableRange<float>(1000, 2000, 1, 1), 2000));

    return Layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}
