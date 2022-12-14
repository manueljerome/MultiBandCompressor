/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once
/*
* Roadmap, 
* 1.) split audio into 3 bands - check
* 2.) create parameters to control where this split happens. -check
* 3.) check that splitting into 3 bands produces no audible artifacts. -check
* 4.) create audio paramters for the 3 band compressors. these need to live in the 
* band instances. -check
* 5.) add 2 remaining compressors. -check
* 6.) add ability to mute/solo/bypass individual compressors. - check
* 7.) add input and output gain to offset changes in output levels. -check
*/
#include <JuceHeader.h>
namespace Params{
    enum Names {
        Low_Mid_Crossover_Freq,
        Mid_High_Crossover_Freq,

        Threshold_Low_Band,
        Threshold_Mid_Band,
        Threshold_High_Band, 

        Attack_Low_Band,
        Attack_Mid_Band,
        Attack_High_Band, 

        Release_Low_Band, 
        Release_Mid_Band,
        Release_High_Band, 

        Ratio_Low_Band, 
        Ratio_Mid_Band,
        Ratio_High_Band, 

        Bypassed_Low_Band, 
        Bypassed_Mid_Band, 
        Bypassed_High_Band,

        Mute_Low_Band,
        Mute_Mid_Band,
        Mute_High_Band,

        Solo_Low_Band,
        Solo_Mid_Band,
        Solo_High_Band, 

        Input_Gain,
        Output_Gain
        
     };

    inline const std::map<Names, juce::String>& GetParams() {
        static std::map<Names, juce::String> params = {
            {Low_Mid_Crossover_Freq,"Low Mid Crossover Freq"},
            {Mid_High_Crossover_Freq, "Mid High Crossover Freq" },
            {Threshold_Low_Band, "Threshold Low Band"},
            {Threshold_Mid_Band, "Threshold Mid Band"},
            {Threshold_High_Band, "Threshold High Band"}, 
            {Release_Low_Band, "Release Low Band"},
            {Release_Mid_Band, "Release Mid Band"},
            {Release_High_Band, "Release High Band"},
            {Ratio_Low_Band, "Ratio Low Band"},
            {Ratio_Mid_Band, "Ratio Mid Band"},
            {Ratio_High_Band, "Ratio High Band"},
            {Bypassed_Low_Band,"Bypassed Low Band"},
            {Bypassed_Mid_Band, "Bypassed Mid Band"},
            {Bypassed_High_Band, "Bypassed High Band"},
            {Mute_Low_Band,"Mute Low Band"},
            {Mute_Mid_Band, "Mute Mid Band"},
            {Mute_High_Band, "Mute High Band"},
            {Solo_Low_Band,"Solo Low Band"},
            {Solo_Mid_Band, "Solo Mid Band"},
            {Solo_High_Band, "Solo High Band"},
            {Input_Gain, "Input Gain"},
            {Output_Gain, "Output Gain"}
        };
        return params;
    }
}
struct CompressorBand {
    juce::AudioParameterFloat* Attack{ nullptr };
    juce::AudioParameterFloat* Release{ nullptr };
    juce::AudioParameterFloat* Threshold{ nullptr };
    juce::AudioParameterChoice* ratio{ nullptr };
    juce::AudioParameterBool* Bypassed{ nullptr };
    juce::AudioParameterBool* Mute{ nullptr };
    juce::AudioParameterBool* Solo{ nullptr };

    void Prepare(juce::dsp::ProcessSpec& spec) {
        compressor.prepare(spec);
    }

    void UpdateCompressorSettings() {
        compressor.setAttack(Attack->get());
        compressor.setRelease(Release->get());
        compressor.setThreshold(Threshold->get());
        compressor.setRatio(ratio->getCurrentChoiceName().getFloatValue());
    }

    void Process(juce::AudioBuffer<float>& buffer) {
        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto context = juce::dsp::ProcessContextReplacing<float>(block);

        compressor.process(context);
    }
private:
    juce::dsp::Compressor<float> compressor;
};

//==============================================================================
/**
*/
class NewProjectAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{

   
public:
    //==============================================================================
    NewProjectAudioProcessor();
    ~NewProjectAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    using APVTS = juce::AudioProcessorValueTreeState;
    static APVTS::ParameterLayout createParameterLayout();
    APVTS apvts{ *this, nullptr, "Parameters", createParameterLayout()};

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewProjectAudioProcessor)

        std::array<CompressorBand,3> compressors;
    CompressorBand& lowCompBand = compressors[0];
    CompressorBand& midCompBand = compressors[1];
    CompressorBand& highCompBand = compressors[2];
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    Filter  LP1, AP2,
            HP1, LP2,
                 HP2;

    juce::AudioParameterFloat* lowMidCrossover{nullptr};
    juce::AudioParameterFloat* midHighCrossover{ nullptr };
    std::array<juce::AudioBuffer<float>, 3> FilterBuffer;

    juce::AudioParameterFloat* inputGain{ nullptr };
    juce::AudioParameterFloat* outputGain{ nullptr };
    juce::dsp::Gain<float> inGain, outGain;

    template<typename T, typename U>
    void applyGain(T& buffer, U& Gain) {
        auto& block = juce::dsp::AudioBlock<float>(buffer);
        auto cxt = juce::dsp::ProcessContextReplacing<float>(block);
        Gain.process(ctx);
    }
};
