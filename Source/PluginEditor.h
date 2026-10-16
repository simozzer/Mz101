// Mz101 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

//
// The look of the panel: long-throw sliders with a 0-10 scale beside them, and slide
// switches whose positions are named beside them, as on the original.
//
class PanelLook final : public juce::LookAndFeel_V4
{
public:
    PanelLook();

    int getSliderThumbRadius(juce::Slider&) override { return thumbRadius; }

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool highlighted, bool down) override;

    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return juce::FontOptions(11.5f, juce::Font::bold);
    }

    static constexpr int thumbRadius = 9;
};

//
// MIDI learn, from the panel: right-click a control for its menu, and its hover hint names
// the CC it answers to now.
//
namespace midiLearn
{
    void showMenu(Mz101Processor&, int index);
    juce::String hint(const Mz101Processor&, int index);
}

// A slider whose right button opens the MIDI learn menu instead of moving it, and whose hint
// is worked out when shown, so it names the CC as it is now.
class LearnSlider final : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    std::function<juce::String()> hint;

    juce::String getTooltip() override { return hint ? hint() : juce::String(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        Slider::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDrag(e); }
    void mouseUp(const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu()) Slider::mouseUp(e); }
};

//
// One control: its legend above, the slider or switch, and for a switch the name of each
// position beside it.
//
class PanelControl final : public juce::Component, public juce::TooltipClient
{
public:
    PanelControl(Mz101Processor&, const juce::String& paramId,
                 const juce::String& legend, juce::StringArray positionNames = {});

    bool isSwitch() const { return ! positions.isEmpty(); }
    int preferredWidth() const { return isSwitch() ? 66 : 42; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;   // right-click on the legend, too

    juce::String getTooltip() override { return midiLearn::hint(processor, index); }

    juce::Slider& getSlider() { return slider; }

private:
    Mz101Processor& processor;
    int index;
    LearnSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String legend;
    juce::StringArray positions;
};

// HOLD, with the same right-click menu and hint.
class LearnButton final : public juce::TextButton
{
public:
    LearnButton(Mz101Processor&, const juce::String& paramId, const juce::String& text);

    juce::String getTooltip() override { return midiLearn::hint(processor, index); }

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    Mz101Processor& processor;
    int index;
};

//
// KEY TRANSPOSE and LEGATO, and the step readout - "12/100" while loading, the step playing
// while it plays. As on the hardware, KEY TRANSPOSE is REST while in LOAD.
//
class SeqButtons final : public juce::Component, private juce::Timer
{
public:
    explicit SeqButtons(Mz101Processor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    Mz101Processor& processor;
    juce::TextButton transpose { "TRANS/REST" }, legato { "LEGATO" };
    juce::String readout;
};

//
// The whole front panel, at one fixed design size. The editor scales it to whatever size
// the window is, so the layout is written once in panel coordinates.
//
class Panel final : public juce::Component, private juce::Timer
{
public:
    explicit Panel(Mz101Processor&);
    ~Panel() override;

    static constexpr int designWidth = 1490;
    static constexpr int designHeight = 610;

    void paint(juce::Graphics&) override;
    void resized() override;

    PanelLook& getLook() { return look; }

private:
    struct Section
    {
        juce::String title;
        std::vector<juce::Component*> items;
        juce::Rectangle<int> bounds;
    };

    // A control learning, or a CC changing - which may come from the audio thread - repaints.
    void timerCallback() override;
    int seenLearning = -1, seenVersion = -1;

    PanelControl* add(const char* id, const char* legend, juce::StringArray positions = {});
    Section& section(const char* title, std::initializer_list<juce::Component*> items);
    // Lays out one row of sections from x; returns where it ended. With fillTo > 0 the
    // gaps grow so the row ends there.
    int layoutRow(std::vector<Section*> row, int x, int y, int height, int fillTo);

    Mz101Processor& processor;
    PanelLook look;

    std::vector<std::unique_ptr<PanelControl>> controls;
    std::vector<std::unique_ptr<Section>> sections;
    std::vector<Section*> topRow, bottomRow;

    SeqButtons seqButtons;
    LearnButton holdButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAttachment;

    juce::ComboBox presetBox;
    juce::MidiKeyboardComponent keyboard;
    juce::Rectangle<int> keyboardSection;
};

class Mz101Editor final : public juce::AudioProcessorEditor
{
public:
    explicit Mz101Editor(Mz101Processor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Panel panel;

    // Hover hints. A child of the editor rather than the desktop, as hosts prefer, and
    // declared after the panel so it goes before the look it borrows from it.
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mz101Editor)
};
