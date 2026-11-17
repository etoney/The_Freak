#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "CDLink.h"
#include "FreakEngine.h"
#include "Presets.h"

class FreakProcessor : public juce::AudioProcessor
{
public:
    FreakProcessor();
    ~FreakProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "The Freak"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String bandId (int band, const char* what) { return "b" + juce::String (band + 1) + what; }
    static juce::StringArray linkIds();
    juce::AudioProcessorValueTreeState apvts;
    freak::Params currentParams() const;

    freak::Engine& getEngine() { return engine; }

    // output samples (mono) for the editor's spectrum
    juce::AbstractFifo specFifo { 8192 };
    std::vector<float> specBuf = std::vector<float> (8192);

    // Castle Danger Link: The Master can read and set The Freak's bands
    using Link = cdlink::PluginLink<juce::AudioProcessorValueTreeState>;
    Link& getLink() { return *link; }

    // ---- user presets (files shared by every project and DAW)
    static juce::File getUserPresetFolder();
    static juce::Array<juce::File> getUserPresets();
    static juce::File fileForPresetName (const juce::String& name);
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String getCurrentPresetName() const { return currentPresetName; }
    std::atomic<int> presetVersion { 0 };

    // The Master's notes for each band ("why"), shown under the band controls
    juce::StringArray bandNotes = juce::StringArray ("", "", "", "", "", "");

private:
    std::unique_ptr<Link> link;
    void resetToDefaults();
    freak::Engine engine;
    int currentProgram = 0;
    juce::String currentPresetName { "Init (All Bands Off)" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreakProcessor)
};
