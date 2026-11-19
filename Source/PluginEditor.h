#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "Rebel.h"

class FreakLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FreakLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hi, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hi, bool down) override;
    juce::Image rustDisc;
};

// Segmented row of rusty buttons bound to a choice parameter; can be re-bound to another band's parameter
class Selector : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Selector (juce::StringArray labels);
    void bind (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    int getIndex() const { return index; }
    std::function<void()> onChange;
private:
    int segmentAt (juce::Point<int>) const;
    juce::StringArray labels;
    std::unique_ptr<juce::ParameterAttachment> att;
    int index = 0, hover = -1;
};

// The Freak's eye: looks around, and goes bloodshot and wide the harder the bands are working
class FreakEye : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void advance (float activityDb, float dt);
private:
    float lookX = 0, lookY = 0, targetX = 0, targetY = 0, heat = 0, blink = 0, t = 0, nextLook = 1.0f;
};

// Big display: output spectrum, each band's static curve, the live (moving) curve, draggable band nodes
class CurveView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit CurveView (FreakProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void pullSpectrum();                 // call from the editor timer
    int selected = 0;
    std::function<void (int band)> onSelect;
    std::array<float, freak::kBands> dyn {};

    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    juce::Point<float> nodePos (int band) const;

private:
    int nodeAt (juce::Point<float>) const;
    juce::RangedAudioParameter* param (int band, const char* what) const;
    float value (int band, const char* what) const;
    void setValue (int band, const char* what, float v);
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (30.0f, 10.0f).withTrimmedBottom (14.0f); }

    FreakProcessor& proc;
    int hover = -1, dragging = -1;
    bool dragDepth = false;
    float dragStartDepth = 0, dragStartY = 0;
    static constexpr int kOrder = 12, kSize = 1 << kOrder;
    juce::dsp::FFT fft { kOrder };
    std::vector<float> ring = std::vector<float> (kSize, 0.0f), work = std::vector<float> (kSize * 2, 0.0f), spec = std::vector<float> (kSize / 2, -120.0f);
    int ringPos = 0;
};

// Six band tabs: number, type, frequency, live move. Click to select.
class BandTabs : public juce::Component
{
public:
    explicit BandTabs (FreakProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    int selected = 0;
    std::function<void (int)> onSelect;
    std::array<float, freak::kBands> dyn {};
private:
    int rowAt (int y) const { return juce::jlimit (0, freak::kBands - 1, y * freak::kBands / juce::jmax (1, getHeight())); }
    FreakProcessor& proc;
};

// IN / OUT level bars
class LevelBars : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    float in = 0, out = 0;
};

// In-window modal dialog (message, confirm, or text entry), drawn over the editor
class FreakDialog : public juce::Component
{
public:
    FreakDialog (const juce::String& title, const juce::String& message, bool withTextField,
                 const juce::String& initialText, juce::StringArray buttons,
                 std::function<void (int buttonIndex, const juce::String& text)> onClose);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override {}
private:
    void finish (int index);
    juce::String title, message;
    juce::TextEditor field;
    bool hasField;
    juce::OwnedArray<juce::TextButton> buttons;
    std::function<void (int, const juce::String&)> onClose;
    juce::Rectangle<int> panel;
};

class FreakEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FreakEditor (FreakProcessor&);
    ~FreakEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void selectBand (int band);
    int getSelectedBand() const { return selected; }
    juce::TextButton& getSaveButton() { return saveButton; }
    void pumpForSnapshot() { timerCallback(); }   // used by the headless snapshot test

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::String suffix;   // band parameter suffix ("f", "q"...) or empty for a global knob
    };
    struct Plate { juce::String title; juce::Rectangle<int> bounds; bool hot = false; };

    Knob& makeKnob (const juce::String& text);
    void bindKnob (Knob&, const juce::String& id);
    void placeKnob (Knob&, juce::Rectangle<int>);
    void timerCallback() override;
    void buildBackdrop();
    void refreshBandPlate();

    void refreshPresetBox();
    void stepPreset (int delta);
    void promptSave();
    void confirmDelete();
    void showDialog (const juce::String& title, const juce::String& message, bool withTextField, const juce::String& initialText,
                     juce::StringArray buttons, std::function<void (int, const juce::String&)> onResult);
    void showMessage (const juce::String& title, const juce::String& text);

    FreakProcessor& proc;
    FreakLookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *freqKnob = nullptr, *qKnob = nullptr, *gainKnob = nullptr, *depthKnob = nullptr, *thrKnob = nullptr;
    Knob *amountKnob = nullptr, *speedKnob = nullptr, *outKnob = nullptr;
    Selector typeSel { { "BELL", "LOW SHELF", "HIGH SHELF" } };
    juce::ToggleButton onButton { "ON" }, listenButton { "LISTEN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    FreakEye eye;
    CurveView curve;
    BandTabs tabs;
    LevelBars levels;
    juce::Label noteLabel;
    juce::TooltipWindow tooltips { this, 700 };

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, deleteButton { "DELETE" };
    juce::Array<juce::File> userFiles;
    int lastPresetVersion = -1;
    std::unique_ptr<FreakDialog> dialog;
    static constexpr int kUserIdBase = 1001;

    int selected = 0;
    std::vector<Plate> plates;
    juce::Rectangle<int> logoArea, badgeArea, crestArea, presetArea, stripeArea, linkArea, bandTitleArea;
    int linkTick = 0;
    juce::Image backdrop, logoImage;
    float inMeter = 0, outMeter = 0, flicker = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreakEditor)
    cdrebel::Button rebel { proc.apvts };   // REBEL: The Master hands off
};
