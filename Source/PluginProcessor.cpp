#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "Presets.h"

Mz101Processor::Mz101Processor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "Mz101", params::createLayout()),
      reader(state)
{
    jassert(params::specs().size() <= maxControls);

    for (const auto& p : params::specs())
    {
        jassert(params::learnable(p.cc));
        controls.push_back(state.getParameter(p.id));
    }

    resetAllCcs();
}

void Mz101Processor::assignCc(int index, int cc)
{
    if (index < 0 || index >= (int) controls.size()) return;

    // whoever had this CC gives it up
    if (cc >= 0)
        for (size_t i = 0; i < controls.size(); ++i)
            if ((int) i != index && ccOf[i].load() == cc)
                ccOf[i] = -1;

    ccOf[(size_t) index] = cc;
    ++mapVersion;
}

void Mz101Processor::resetAllCcs()
{
    for (size_t i = 0; i < controls.size(); ++i)
        ccOf[i] = params::specs()[i].cc;
    ++mapVersion;
}

void Mz101Processor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    engine.setSettings(reader.read());
    keyboardState.reset();
}

bool Mz101Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void Mz101Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer(midi, 0, n, true);
    engine.setSettings(reader.read());

    // The EXT CLK input: where the host's timeline is at the top of this block. The engine
    // runs it forward sample by sample from there, and is put right again every block - so
    // a loop jump or a tempo change is followed within one block.
    {
        bool available = false, playing = false;
        double ppq = 0.0, bpm = 120.0;

        if (auto* head = getPlayHead())
            if (const auto pos = head->getPosition())
                if (const auto p = pos->getPpqPosition())
                {
                    available = true;
                    playing = pos->getIsPlaying();
                    ppq = *p;
                    if (const auto b = pos->getBpm()) bpm = *b;
                }

        engine.setHostClock(available, playing, ppq, bpm);
    }

    for (int r = transposePresses.exchange(0); r > 0; --r) engine.keyTranspose();
    for (int l = legatoPresses.exchange(0); l > 0; --l)    engine.legato();

    buffer.clear();
    if (buffer.getNumChannels() == 0) return;

    // One voice, one channel: render mono, and split the block at each MIDI event so a note
    // lands on its sample rather than at the top of the block.
    float* out = buffer.getWritePointer(0);
    int pos = 0;

    for (const auto meta : midi)
    {
        const int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > pos)
        {
            engine.render(out + pos, at - pos);
            pos = at;
        }
        handle(meta.getMessage());
    }

    if (pos < n) engine.render(out + pos, n - pos);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, n);
}

void Mz101Processor::handle(const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn(m.getNoteNumber());
    else if (m.isNoteOff())
        engine.noteOff(m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.setPitchBend((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isControllerOfType(1))
        engine.setModLever((float) m.getControllerValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();

        // MIDI learn: the first learnable CC to arrive is taken, and does nothing else.
        int waiting = learning.load();
        if (waiting >= 0 && params::learnable(cc) && learning.compare_exchange_strong(waiting, -1))
        {
            assignCc(waiting, cc);
            return;
        }

        // A mapped CC moves the parameter itself - so the panel follows, the host can record
        // it, and the project saves it - and takes effect from this sample, not the next block.
        // 0-127 spans the control's whole travel; a switch takes the nearest position.
        for (size_t i = 0; i < controls.size(); ++i)
        {
            if (ccOf[i].load(std::memory_order_relaxed) == cc)
            {
                controls[i]->setValueNotifyingHost((float) m.getControllerValue() / 127.0f);
                engine.setSettings(reader.read());
                break;
            }
        }
    }
}

juce::AudioProcessorEditor* Mz101Processor::createEditor()
{
    return new Mz101Editor(*this);
}

int Mz101Processor::getNumPrograms()
{
    return (int) presets::all().size();
}

void Mz101Processor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms()) return;
    currentProgram = index;
    presets::apply(presets::all()[(size_t) index], state);
}

const juce::String Mz101Processor::getProgramName(int index)
{
    if (index < 0 || index >= getNumPrograms()) return {};
    return presets::all()[(size_t) index].name;
}

void Mz101Processor::getStateInformation(juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    copy.setProperty("program", currentProgram, nullptr);

    // The sequence is not a parameter - nothing to automate - so it rides along as text.
    juce::StringArray stepList;
    for (int i = 0; i < engine.seqLength(); ++i)
        stepList.add(juce::String((int) engine.seqStep(i)));
    copy.setProperty("sequence", stepList.joinIntoString(","), nullptr);

    // The CC assignments, as id=cc (-1 for none), so a control added or removed later
    // cannot shift anyone else's.
    juce::StringArray map;
    for (size_t i = 0; i < controls.size(); ++i)
        map.add(juce::String(params::specs()[i].id) + "=" + juce::String(ccOf[i].load()));
    copy.setProperty("midiMap", map.joinIntoString(","), nullptr);

    if (auto xml = copy.createXml())
        copyXmlToBinary(*xml, destData);
}

void Mz101Processor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml(*xml);
            currentProgram = tree.getProperty("program", 0);

            juce::StringArray stepList;
            stepList.addTokens(tree.getProperty("sequence").toString(), ",", {});
            stepList.removeEmptyStrings();
            std::vector<uint32_t> loaded;
            for (const auto& s : stepList)
                loaded.push_back((uint32_t) s.getIntValue());
            engine.loadSequence(loaded.data(), (int) loaded.size());

            // CC assignments: the defaults, then whatever the project changed. A project from
            // before MIDI learn has none, and keeps the defaults.
            learning = -1;
            resetAllCcs();
            juce::StringArray map;
            map.addTokens(tree.getProperty("midiMap").toString(), ",", {});
            for (const auto& entry : map)
            {
                const int index = params::indexOf(entry.upToFirstOccurrenceOf("=", false, false));
                const int cc = entry.fromFirstOccurrenceOf("=", false, false).getIntValue();
                if (index >= 0 && (cc == -1 || params::learnable(cc)))
                    ccOf[(size_t) index] = cc;
            }
            ++mapVersion;

            state.replaceState(tree);

            // Entering LOAD wipes the sequence to record a new one. A project saved
            // mid-recording would wipe its own sequence on reopening, so it reopens at OFF.
            if (auto* seq = state.getParameter("seqMode"))
                if (juce::roundToInt(seq->convertFrom0to1(seq->getValue())) == sh101::Seq::Load)
                    seq->setValueNotifyingHost(seq->convertTo0to1((float) sh101::Seq::Off));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mz101Processor();
}
