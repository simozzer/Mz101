// Mz101 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "PluginEditor.h"

#include "Params.h"
#include "Presets.h"

namespace colours
{
    const juce::Colour panel    { 0xff1f2023 };
    const juce::Colour section  { 0xff2b2c30 };
    const juce::Colour edge     { 0xff3c3e43 };
    const juce::Colour legend   { 0xffe9e9e6 };
    const juce::Colour dim      { 0xff9a9ca1 };
    const juce::Colour slot     { 0xff050505 };
    const juce::Colour cap      { 0xffd8d8d4 };
    const juce::Colour accent   { 0xffd2382f };   // the 101's red
}

//============================================================================== PanelLook

PanelLook::PanelLook()
{
    setColour(juce::ComboBox::backgroundColourId, colours::section);
    setColour(juce::ComboBox::outlineColourId, colours::edge);
    setColour(juce::ComboBox::textColourId, colours::legend);
    setColour(juce::ComboBox::arrowColourId, colours::legend);
    setColour(juce::PopupMenu::backgroundColourId, colours::section);
    setColour(juce::PopupMenu::textColourId, colours::legend);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour(juce::TextButton::textColourOffId, colours::legend);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::BubbleComponent::backgroundColourId, colours::section);
    setColour(juce::BubbleComponent::outlineColourId, colours::edge);
    setColour(juce::TooltipWindow::textColourId, colours::legend);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff111214));
    setColour(juce::TooltipWindow::outlineColourId, colours::accent);
}

void PanelLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                 float sliderPos, float, float,
                                 juce::Slider::SliderStyle, juce::Slider& slider)
{
    const bool isSwitch = slider.getProperties()["switch"];
    const float cx = (float) x + (float) width * 0.5f;
    const float top = (float) y, bottom = (float) (y + height);

    // the scale: 0 to 10 for a slider, a mark per position for a switch
    g.setColour(colours::dim);
    const int marks = isSwitch ? juce::roundToInt(slider.getMaximum() - slider.getMinimum()) : 10;
    for (int i = 0; i <= marks; ++i)
    {
        const float ty = bottom - (bottom - top) * (float) i / (float) marks;
        const bool major = isSwitch || i % 5 == 0;
        g.fillRect(cx - (major ? 17.0f : 14.0f), ty - 0.5f, major ? 8.0f : 5.0f, 1.0f);
    }

    // the slot
    g.setColour(colours::slot);
    g.fillRoundedRectangle(cx - 2.5f, top - 6.0f, 5.0f, (bottom - top) + 12.0f, 2.0f);

    // the cap
    if (isSwitch)
    {
        const juce::Rectangle<float> cap(cx - 8.0f, sliderPos - 7.0f, 16.0f, 14.0f);
        g.setColour(juce::Colour(0xff121212));
        g.fillRoundedRectangle(cap, 2.0f);
        g.setColour(juce::Colour(0xff4a4a4a));
        g.drawRoundedRectangle(cap, 2.0f, 1.0f);
        g.setColour(colours::legend);
        g.fillRect(cap.getX() + 3.0f, sliderPos - 0.75f, cap.getWidth() - 6.0f, 1.5f);
    }
    else
    {
        const juce::Rectangle<float> cap(cx - 11.0f, sliderPos - 8.0f, 22.0f, 16.0f);
        g.setGradientFill(juce::ColourGradient(colours::cap, cap.getX(), cap.getY(),
                                               colours::cap.darker(0.35f), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle(cap, 2.5f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(cap, 2.5f, 1.0f);
        g.setColour(juce::Colour(0xff202020));
        g.fillRect(cap.getX() + 2.0f, sliderPos - 1.0f, cap.getWidth() - 4.0f, 2.0f);
    }
}

void PanelLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                     bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(down ? juce::Colour(0xff0c0c0c) : juce::Colour(0xff161616));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(highlighted ? colours::dim : colours::edge);
    g.drawRoundedRectangle(r, 3.0f, 1.0f);

    // the LED
    const auto led = juce::Rectangle<float>(6.0f, 6.0f).withCentre({ r.getCentreX(), r.getY() + 7.0f });
    g.setColour(b.getToggleState() ? colours::accent.brighter(0.4f) : juce::Colour(0xff3a1714));
    g.fillEllipse(led);
}

//============================================================================== MIDI learn

namespace midiLearn
{

juce::String hint(const Mz101Processor& p, int index)
{
    return params::hintFor(index, p.ccFor(index), p.learningIndex() == index);
}

void showMenu(Mz101Processor& p, int index)
{
    const int cc = p.ccFor(index);
    const int def = params::specs()[(size_t) index].cc;
    const bool learning = p.learningIndex() == index;

    juce::PopupMenu m;
    m.addSectionHeader(juce::String(params::specs()[(size_t) index].name) +
                       (cc >= 0 ? "  -  CC " + juce::String(cc) : "  -  no CC"));

    if (learning) m.addItem("Cancel MIDI Learn", [&p] { p.learn(-1); });
    else          m.addItem("MIDI Learn", [&p, index] { p.learn(index); });

    m.addItem("Forget CC", cc >= 0, false, [&p, index] { p.forgetCc(index); });
    m.addItem("Restore default (CC " + juce::String(def) + ")", cc != def, false,
              [&p, index] { p.restoreDefaultCc(index); });
    m.addSeparator();
    m.addItem("Reset all CCs to defaults", [&p] { p.resetAllCcs(); });

    m.showMenuAsync(juce::PopupMenu::Options());
}

// While a control waits for its CC: a red outline and LEARN.
void drawLearning(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(colours::accent);
    g.drawRoundedRectangle(r.reduced(1.0f), 4.0f, 2.0f);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("LEARN", r.removeFromBottom(12.0f), juce::Justification::centred);
}

} // namespace midiLearn

//============================================================================== LearnButton

LearnButton::LearnButton(Mz101Processor& p, const juce::String& paramId, const juce::String& text)
    : juce::TextButton(text), processor(p), index(params::indexOf(paramId))
{
}

void LearnButton::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { midiLearn::showMenu(processor, index); return; }
    TextButton::mouseDown(e);
}

void LearnButton::mouseUp(const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) TextButton::mouseUp(e);
}

void LearnButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    TextButton::paintButton(g, highlighted, down);
    if (processor.learningIndex() == index)
    {
        g.setColour(colours::accent);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 3.0f, 2.0f);
    }
}

//============================================================================== PanelControl

PanelControl::PanelControl(Mz101Processor& p, const juce::String& paramId,
                           const juce::String& legendText, juce::StringArray positionNames)
    : processor(p), index(params::indexOf(paramId)),
      legend(legendText), positions(std::move(positionNames))
{
    auto& state = p.state;
    jassert(index >= 0);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.getProperties().set("switch", isSwitch());
    slider.setVelocityBasedMode(false);
    if (isSwitch())
        slider.onValueChange = [this] { repaint(); };   // the lit position name follows the cap
    else
    {
        slider.setPopupDisplayEnabled(true, true, nullptr);
        slider.setDoubleClickReturnValue(true, 0.0);
    }
    addAndMakeVisible(slider);

    // the same hint and menu over the slider as over its legend and position names
    slider.hint = [this] { return getTooltip(); };
    slider.onRightClick = [this] { midiLearn::showMenu(processor, index); };

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, paramId, slider);

    // the attachment has set the range by now
    if (! isSwitch())
        slider.setDoubleClickReturnValue(true, (double) state.getParameter(paramId)->convertFrom0to1(
                                                   state.getParameter(paramId)->getDefaultValue()));
}

void PanelControl::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(30);
    r.removeFromBottom(4);

    if (isSwitch()) slider.setBounds(r.removeFromLeft(36));
    else            slider.setBounds(r.withSizeKeepingCentre(40, r.getHeight()));
}

void PanelControl::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) midiLearn::showMenu(processor, index);
}

void PanelControl::paint(juce::Graphics& g)
{
    if (processor.learningIndex() == index)
        midiLearn::drawLearning(g, getLocalBounds().toFloat());

    g.setColour(colours::legend);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawFittedText(legend, getLocalBounds().removeFromTop(28), juce::Justification::centredBottom, 2);

    if (! isSwitch()) return;

    // the position names, level with where the cap sits in each
    const auto track = slider.getBounds().reduced(0, PanelLook::thumbRadius);
    const int n = positions.size();

    g.setFont(juce::FontOptions(10.0f));
    for (int i = 0; i < n; ++i)
    {
        const float ty = (float) track.getBottom() - (float) track.getHeight() * (float) i / (float) juce::jmax(1, n - 1);
        g.setColour(juce::roundToInt(slider.getValue()) == i ? colours::legend : colours::dim);
        g.drawText(positions[i], juce::Rectangle<float>((float) slider.getRight() - 4.0f, ty - 7.0f,
                                                        (float) getWidth() - (float) slider.getRight() + 4.0f, 14.0f),
                   juce::Justification::centredLeft);
    }
}

//============================================================================== SeqButtons

SeqButtons::SeqButtons(Mz101Processor& p) : processor(p)
{
    transpose.onClick = [this] { processor.pressKeyTranspose(); };
    legato.onClick = [this] { processor.pressLegato(); };
    transpose.setTooltip("KEY TRANSPOSE: press, then a key - the shift from middle C moves the VCO. In LOAD: enter a rest");
    legato.setTooltip("In LOAD: tie the next note to this one - no new envelope, and it glides with portamento AUTO");
    addAndMakeVisible(transpose);
    addAndMakeVisible(legato);
    startTimerHz(15);
}

void SeqButtons::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(12);
    transpose.setBounds(r.removeFromTop(40));
    r.removeFromTop(10);
    legato.setBounds(r.removeFromTop(40));
}

void SeqButtons::timerCallback()
{
    const auto& e = processor.engineView();
    const int mode = juce::roundToInt(processor.state.getRawParameterValue("seqMode")->load());

    juce::String text;
    if (mode == sh101::Seq::Load)
        text = juce::String(e.seqLength()) + "/" + juce::String(sh101::Engine::maxSteps);
    else if (mode == sh101::Seq::Play && e.seqPosition() >= 0)
        text = juce::String(e.seqPosition() + 1) + "/" + juce::String(e.seqLength());
    else if (e.keyShift() != 0)
        text = "KEY " + juce::String(e.keyShift() > 0 ? "+" : "") + juce::String(e.keyShift());
    else
        text = juce::String(e.seqLength()) + " STEPS";

    if (e.keyTransposeArmed())
        text = "KEY?";

    // the LEDs: a tie armed for the next note; KEY TRANSPOSE waiting for its key
    if (legato.getToggleState() != e.seqLegatoArmed())
        legato.setToggleState(e.seqLegatoArmed(), juce::dontSendNotification);
    if (transpose.getToggleState() != e.keyTransposeArmed())
        transpose.setToggleState(e.keyTransposeArmed(), juce::dontSendNotification);

    if (text != readout)
    {
        readout = text;
        repaint();
    }
}

void SeqButtons::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom(40).reduced(0, 6);
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRoundedRectangle(r.toFloat(), 3.0f);
    g.setColour(colours::accent.brighter(0.3f));
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
    g.drawText(readout, r, juce::Justification::centred);
}

//============================================================================== Panel

Panel::Panel(Mz101Processor& p)
    : processor(p),
      seqButtons(p),
      holdButton(p, "arpHold", "HOLD"),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&look);
    startTimerHz(15);

    // the 101's own wording, top to bottom on the panel; positions are bottom first
    // RATE reads as a note length while SYNC is on HOST
    auto* rate = add("lfoRate", "RATE");
    rate->getSlider().textFromValueFunction = [this] (double v)
    {
        if (juce::roundToInt(processor.state.getRawParameterValue("lfoSync")->load()) == sh101::LfoSync::Host)
            return juce::String(sh101::syncDivisions[sh101::syncDivisionIndex((float) v)].name);
        return juce::String(v, 1);
    };

    auto& mod = section("MODULATOR", {
        rate,
        add("lfoSync",   "SYNC", { "FREE", "HOST" }),
        add("lfoWave",   "WAVE", { "TRI", "SQR", "RAND", "NOISE" }),
        add("clockSource", "CLOCK", { "INT", "HOST" }),
        add("clockDiv",  "DIV", { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }) });

    auto& vco = section("VCO", {
        add("vcoMod",    "MOD"),
        add("range",     "RANGE", { "16'", "8'", "4'", "2'" }),
        add("pw",        "PW / PWM"),
        add("pwmSource", "PWM", { "LFO", "MAN", "ENV" }) });

    auto& mixer = section("SOURCE MIXER", {
        add("pulse",     "PULSE"),
        add("saw",       "SAW"),
        add("sub",       "SUB OSC"),
        add("subType",   "SUB", { "-1 OCT", "-2 OCT", "-2 PW" }),
        add("noise",     "NOISE") });

    auto& vcf = section("VCF", {
        add("cutoff",    "FREQ"),
        add("res",       "RES"),
        add("vcfEnv",    "ENV"),
        add("vcfMod",    "MOD"),
        add("vcfKybd",   "KYBD") });

    auto& vca = section("VCA", {
        add("vcaMode",   "MODE", { "ENV", "GATE" }) });

    auto& env = section("ENV", {
        add("attack",    "A"),
        add("decay",     "D"),
        add("sustain",   "S"),
        add("release",   "R"),
        add("envTrig",   "TRIG", { "G+T", "GATE", "LFO" }) });

    auto& porta = section("PORTAMENTO", {
        add("portaMode", "MODE", { "OFF", "AUTO", "ON" }),
        add("portaTime", "TIME") });

    auto& bender = section("BENDER", {
        add("bendVco",   "VCO"),
        add("bendVcf",   "VCF"),
        add("modDepth",  "MOD") });

    auto& arp = section("ARPEGGIO", {
        add("arpMode",   "MODE", { "OFF", "UP", "U&D", "DOWN" }),
        &holdButton });

    auto& seq = section("SEQUENCER", {
        add("seqMode",   "MODE", { "OFF", "PLAY", "LOAD" }),
        &seqButtons });

    auto& transpose = section("KEY", {
        add("transpose", "TRANS", { "L", "M", "H" }) });

    auto& master = section("MASTER", {
        add("tune",      "TUNE"),
        add("volume",    "VOL") });

    topRow    = { &mod, &vco, &mixer, &vcf, &vca, &env };
    bottomRow = { &porta, &bender, &arp, &seq, &transpose, &master };

    addAndMakeVisible(seqButtons);

    holdButton.setClickingTogglesState(true);
    presetBox.setTooltip("Factory patches. Your own edits are saved with the project.");
    holdButton.setConnectedEdges(0);
    addAndMakeVisible(holdButton);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state, "arpHold", holdButton);

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem(processor.getProgramName(i), i + 1);
    presetBox.setSelectedId(processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        processor.setCurrentProgram(presetBox.getSelectedId() - 1);
        processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
    };
    addAndMakeVisible(presetBox);

    //
    // Where to come back to: the version this is, and the page the next one will be on.
    // Nothing here checks for updates by itself - the plugin never goes online unasked.
    // It takes no keyboard focus, or a host's space bar could open it rather than play.
    //
    projectLink.setButtonText("Mz101 " JucePlugin_VersionString "  -  github.com/simozzer/Mz101");
    projectLink.setURL(juce::URL("https://github.com/simozzer/Mz101"));
    projectLink.setFont(juce::FontOptions(12.0f), false, juce::Justification::centredRight);
    projectLink.setColour(juce::HyperlinkButton::textColourId, colours::dim);
    projectLink.setTooltip("The project's page: the latest version and what's new in it, the source, "
                           "and where to report a problem. Opens in your browser.");
    projectLink.setWantsKeyboardFocus(false);
    addAndMakeVisible(projectLink);

    // The 101's 32 keys, F to C.
    keyboard.setAvailableRange(41, 72);
    keyboard.setOctaveForMiddleC(4);   // middle C is C4, as the hints and README count it
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent.withAlpha(0.6f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha(0.25f));
    addAndMakeVisible(keyboard);
}

void Panel::timerCallback()
{
    const int l = processor.learningIndex(), v = processor.midiMapVersion();
    if (l == seenLearning && v == seenVersion) return;

    seenLearning = l;
    seenVersion = v;
    for (auto& c : controls) c->repaint();
    holdButton.repaint();
}

Panel::~Panel()
{
    setLookAndFeel(nullptr);
}

PanelControl* Panel::add(const char* id, const char* legend, juce::StringArray positions)
{
    controls.push_back(std::make_unique<PanelControl>(processor, id, legend, std::move(positions)));
    addAndMakeVisible(*controls.back());
    return controls.back().get();
}

Panel::Section& Panel::section(const char* title, std::initializer_list<juce::Component*> items)
{
    sections.push_back(std::make_unique<Section>());
    sections.back()->title = title;
    sections.back()->items = items;
    return *sections.back();
}

int Panel::layoutRow(std::vector<Section*> row, int x, int y, int height, int fillTo)
{
    constexpr int pad = 8, header = 24;

    auto widthOf = [] (juce::Component* c)
    {
        if (auto* pc = dynamic_cast<PanelControl*>(c)) return pc->preferredWidth();
        return dynamic_cast<juce::Button*>(c) != nullptr ? 56 : 80;
    };

    // with a right edge to reach, the spare width goes into the gaps between sections
    int gap = 8;
    if (fillTo > 0 && row.size() > 1)
    {
        int total = 0;
        for (auto* s : row)
        {
            total += 2 * pad;
            for (auto* c : s->items) total += widthOf(c);
        }
        gap = juce::jmax(8, (fillTo - x - total) / (int) (row.size() - 1));
    }

    for (auto* s : row)
    {
        int w = pad;
        for (auto* c : s->items)
            w += widthOf(c);
        w += pad;

        s->bounds = { x, y, w, height };

        int cx = x + pad;
        for (auto* c : s->items)
        {
            if (auto* pc = dynamic_cast<PanelControl*>(c))
            {
                pc->setBounds(cx, y + header, pc->preferredWidth(), height - header - pad);
                cx += pc->preferredWidth();
            }
            else if (dynamic_cast<juce::Button*>(c) != nullptr)
            {
                c->setBounds(cx + 4, y + header + 36, 48, 40);
                cx += 56;
            }
            else
            {
                c->setBounds(cx + 4, y + header + 24, 72, height - header - pad - 24);
                cx += 80;
            }
        }

        x += w + gap;
    }

    return x;
}

void Panel::resized()
{
    constexpr int margin = 14;

    presetBox.setBounds(designWidth - margin - 220, 12, 220, 26);
    projectLink.setBounds(presetBox.getX() - 16 - 420, presetBox.getY(), 420, presetBox.getHeight());

    layoutRow(topRow, margin, 54, 300, designWidth - margin);
    const int right = layoutRow(bottomRow, margin, 366, 230, 0);

    keyboardSection = { right, 366, designWidth - margin - right, 230 };
    keyboard.setBounds(keyboardSection.reduced(8).withTrimmedTop(16));
    keyboard.setKeyWidth((float) keyboard.getWidth() / 19.0f);   // 19 white keys, F to C
}

void Panel::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel);

    // the name strip
    g.setColour(colours::accent);
    g.fillRect(0, 0, designWidth, 4);

    g.setColour(colours::legend);
    g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    g.drawText("Mz101", 16, 8, 120, 36, juce::Justification::centredLeft);
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("MONOPHONIC SYNTHESIZER", 112, 8, 300, 38, juce::Justification::centredLeft);

    auto box = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour(colours::section);
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
        g.setColour(colours::edge);
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 4.0f, 1.0f);

        if (title.isNotEmpty())
        {
            g.setColour(colours::legend);
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText(title, r.withHeight(24), juce::Justification::centred);
            g.setColour(colours::edge);
            g.fillRect(r.getX() + 8, r.getY() + 23, r.getWidth() - 16, 1);
        }
    };

    for (auto& s : sections)
        box(s->bounds, s->title);

    box(keyboardSection, {});
}

//============================================================================== Mz101Editor

Mz101Editor::Mz101Editor(Mz101Processor& p)
    : AudioProcessorEditor(&p), panel(p)
{
    tooltips.setLookAndFeel(&panel.getLook());
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, Panel::designWidth, Panel::designHeight);

    setResizable(true, true);
    setResizeLimits(Panel::designWidth / 2, Panel::designHeight / 2,
                    Panel::designWidth * 2, Panel::designHeight * 2);
    getConstrainer()->setFixedAspectRatio((double) Panel::designWidth / Panel::designHeight);
    setSize(Panel::designWidth, Panel::designHeight);
}

void Mz101Editor::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel);
}

void Mz101Editor::resized()
{
    const float scale = (float) getWidth() / (float) Panel::designWidth;
    panel.setTransform(juce::AffineTransform::scale(scale));
}
