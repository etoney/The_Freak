#include "PluginProcessor.h"
#include "Rebel.h"
#include "PluginEditor.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

namespace
{
struct BandDefault { int type; float f, q; };
const BandDefault kDefaults[freak::kBands] {
    { freak::lowShelf, 100.0f, 0.71f }, { freak::bell, 250.0f, 1.0f }, { freak::bell, 600.0f, 1.0f },
    { freak::bell, 2000.0f, 1.0f },     { freak::bell, 4500.0f, 1.0f }, { freak::highShelf, 9000.0f, 0.71f } };
}

juce::StringArray FreakProcessor::linkIds()
{
    juce::StringArray ids;
    for (int b = 0; b < freak::kBands; ++b)
        for (auto* w : { "on", "type", "f", "q", "g", "d", "t" }) ids.add (bandId (b, w));
    ids.addArray ({ "amount", "speed", "output" });
    return ids;
}

FreakProcessor::FreakProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TheFreak", createLayout())
{
    link = std::make_unique<Link> ("freak", "The Freak", JucePlugin_VersionString, apvts, linkIds());
    link->onSetByMaster = [this] (const juce::StringArray&) { currentPresetName = "Set by The Master"; ++presetVersion; };
    link->onOtherMessage = [this] (const juce::var& m)
    {
        // The Master can send a short reason for each band: { type: "freak-notes", notes: ["...", ...] }
        if (m.getProperty ("type", "").toString() != "freak-notes") return;
        if (auto* a = m.getProperty ("notes", {}).getArray())
            for (int i = 0; i < freak::kBands; ++i) bandNotes.set (i, i < a->size() ? (*a)[i].toString() : juce::String());
        ++presetVersion;
    };
    link->extraStatus = [this]
    {
        auto* o = new juce::DynamicObject();
        juce::Array<juce::var> dyn;
        for (auto& d : engine.dynDb) dyn.add ((double) d.load());
        o->setProperty ("dynDb", dyn);
        return juce::var (o);
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout FreakProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
        .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%")
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto hzAttr = AudioParameterFloatAttributes().withLabel ("Hz")
        .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto qAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2); });

    NormalisableRange<float> fr (20.0f, 20000.0f, 0.1f);
    fr.setSkewForCentre (1000.0f);
    NormalisableRange<float> qr (0.3f, 8.0f, 0.01f);
    qr.setSkewForCentre (1.2f);

    for (int b = 0; b < freak::kBands; ++b)
    {
        const auto n = "Band " + String (b + 1) + " ";
        const auto& d = kDefaults[b];
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { bandId (b, "on"), 1 }, n + "On", false));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { bandId (b, "type"), 1 }, n + "Type", StringArray { "Bell", "Low Shelf", "High Shelf" }, d.type));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "f"), 1 }, n + "Freq", fr, d.f, hzAttr));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "q"), 1 }, n + "Q", qr, d.q, qAttr));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "g"), 1 }, n + "Gain", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "d"), 1 }, n + "Depth", NormalisableRange<float> (-18.0f, 12.0f, 0.1f), -6.0f, dbAttr));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "t"), 1 }, n + "Threshold", NormalisableRange<float> (0.0f, 18.0f, 0.1f), 4.0f, dbAttr));
    }
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Speed", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "listen", 1 }, "Listen",
                                                        StringArray { "Off", "Band 1", "Band 2", "Band 3", "Band 4", "Band 5", "Band 6" }, 0));
    p.push_back (cdrebel::makeParam());
    return { p.begin(), p.end() };
}

freak::Params FreakProcessor::currentParams() const
{
    auto v = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    freak::Params p;
    for (int b = 0; b < freak::kBands; ++b)
    {
        auto& bp = p.bands[(size_t) b];
        bp.on = v (bandId (b, "on")) > 0.5f;
        bp.type = (int) v (bandId (b, "type"));
        bp.freq = v (bandId (b, "f"));
        bp.q = v (bandId (b, "q"));
        bp.gainDb = v (bandId (b, "g"));
        bp.depthDb = v (bandId (b, "d"));
        bp.thrDb = v (bandId (b, "t"));
    }
    p.amount = v ("amount") * 0.01f;
    p.speed = v ("speed") * 0.01f;
    p.outputDb = v ("output");
    p.listen = (int) v ("listen");
    return p;
}

bool FreakProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void FreakProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setParams (currentParams());
    setLatencySamples (0);
}

void FreakProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    const int outCh = juce::jmin (2, buffer.getNumChannels());
    if (outCh <= 0) return;
    if (inCh == 1 && outCh > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n); // mono in -> stereo out

    double inPow = 0;
    for (int c = 0; c < outCh; ++c) { auto* d = buffer.getReadPointer (c); for (int i = 0; i < n; ++i) inPow += (double) d[i] * d[i]; }
    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), outCh, n);
    double outPow = 0;
    for (int c = 0; c < outCh; ++c) { auto* d = buffer.getReadPointer (c); for (int i = 0; i < n; ++i) outPow += (double) d[i] * d[i]; }
    link->env.addPowers (inPow / juce::jmax (1, outCh * n), outPow / juce::jmax (1, outCh * n), n);

    // spectrum feed (drops samples if the editor isn't reading)
    if (specFifo.getFreeSpace() >= n)
    {
        const auto w = specFifo.write (n);
        auto put = [&] (int start, int count, int dst)
        {
            for (int i = 0; i < count; ++i)
            {
                float s = buffer.getSample (0, start + i);
                if (outCh > 1) s = 0.5f * (s + buffer.getSample (1, start + i));
                specBuf[(size_t) (dst + i)] = s;
            }
        };
        put (0, w.blockSize1, w.startIndex1);
        put (w.blockSize1, w.blockSize2, w.startIndex2);
    }
    for (int ch = outCh; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
}

//==============================================================================
int FreakProcessor::getNumPrograms() { return (int) freak::factoryPresets().size(); }

const juce::String FreakProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? freak::factoryPresets()[(size_t) index].name : "";
}

void FreakProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID() != "rebel") rp->setValueNotifyingHost (rp->getDefaultValue());   // a preset never changes REBEL
    for (int i = 0; i < freak::kBands; ++i) bandNotes.set (i, {});
}

void FreakProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    currentPresetName = freak::factoryPresets()[(size_t) index].name;
    resetToDefaults();
    for (auto& [id, v] : freak::factoryPresets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    ++presetVersion;
}

juce::File FreakProcessor::getUserPresetFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory); // ~/Library on macOS, %APPDATA% on Windows
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("Castle Danger").getChildFile ("The Freak").getChildFile ("Presets");
}

juce::Array<juce::File> FreakProcessor::getUserPresets()
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.freak");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    return files;
}

juce::File FreakProcessor::fileForPresetName (const juce::String& name)
{
    return getUserPresetFolder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".freak");
}

bool FreakProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = rawName.trim();
    if (name.isEmpty()) return false;
    const auto file = fileForPresetName (name);
    if (! file.getParentDirectory().createDirectory()) return false;

    juce::XmlElement xml ("FreakPreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("pluginVersion", JucePlugin_VersionString);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == "listen") continue; // a preset never opens soloing one band
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    if (! xml.writeTo (file)) return false;
    currentPresetName = name;
    ++presetVersion;
    return true;
}

bool FreakProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("FreakPreset")) return false;
    resetToDefaults();
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (e->getStringAttribute ("id")))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) e->getDoubleAttribute ("value")));
    currentPresetName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    ++presetVersion;
    return true;
}

void FreakProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("notes", bandNotes.joinIntoString ("\n"), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void FreakProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            currentPresetName = vt.getProperty ("presetName", getProgramName (currentProgram)).toString();
            auto notes = juce::StringArray::fromLines (vt.getProperty ("notes", "").toString());
            for (int i = 0; i < freak::kBands; ++i) bandNotes.set (i, i < notes.size() ? notes[i] : juce::String());
            vt.removeProperty ("notes", nullptr);
            apvts.replaceState (vt);
            ++presetVersion;
        }
}

juce::AudioProcessorEditor* FreakProcessor::createEditor() { return new FreakEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FreakProcessor(); }
