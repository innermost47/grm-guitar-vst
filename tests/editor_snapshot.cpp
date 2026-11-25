// The editor drawn to a PNG (no host, no window): to look at the layout.
// Usage: editor_snapshot out.png
#include <chrono>
#include <thread>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"
#include "../plugin/PluginEditor.h"
#include "../plugin/views/MixerView.h"

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> plugin(createPluginFilter());
    plugin->setPlayConfigDetails(0, 2, 48000.0, 512);
    plugin->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    for (int k = 0; k < 300; ++k) {
        plugin->processBlock(buf, midi);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto* g = dynamic_cast<GRMGuitarProcessor*>(plugin.get());
    g->addPattern(true);
    auto song = g->getSong();
    song.loop_start = 4.0;          // a loop over bars 2 to 3, a note written past the end
    song.loop_end = 12.0;
    grmg::models::RollNote far;
    far.start = 33.0;
    far.fret = 5;
    song.notes.push_back(far);
    song.bars = 9;
    g->setSong(song);
    // a second guitar with a melody: the first one's notes show faint behind it
    g->addGuitar();
    grmg::Song lead;
    lead.bars = 4;
    const int frets[8] = {3, 5, 7, 8, 7, 5, 3, 2};
    for (int k = 0; k < 8; ++k) {
        grmg::models::RollNote n;
        n.string = 3;
        n.fret = frets[k];
        n.start = 2.0 * k;
        n.length = 1.5;
        lead.notes.push_back(n);
    }
    grmg::models::RollNote dead;
    dead.string = 2;
    dead.fret = 5;
    dead.start = 2.0;
    dead.length = 1.0;
    dead.dead = true;
    lead.notes.push_back(dead);
    for (int i = 0; i < 3; ++i) {          // a shape over three strings, each its own fret
        grmg::models::RollNote b;
        b.string = i;
        b.fret = i == 0 ? 0 : 2;
        b.start = 12.0;
        b.length = 2.0;
        lead.notes.push_back(b);
    }
    lead.stops.push_back(9.0);
    g->setSong(lead);
    g->params.getParameter("midi_mode")->setValueNotifyingHost(1.0f);
    // the second guitar in a free tuning, its low string down to B1: the Strings tab shows it slack
    auto setByHand = [&](const juce::String &id, float value)
    {
        auto *p = g->params.getParameter(GuitarTrack::paramId(1, id));
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(value));
        p->endChangeGesture();
    };
    setByHand("tuning", (float)grmg::CUSTOM_TUNING);
    setByHand(grmg::OPEN_NOTE_PARAMS[0], (float)(35 - grmg::OPEN_NOTE_LOWEST));
    for (int k = 0; k < 100; ++k) { // its networks solved again
        plugin->processBlock(buf, midi);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());
    editor->setSize(1600, 880);
    // let the editor's timers fill the views (the message loop, a few times)
    for (int k = 0; k < 20; ++k) {
        juce::Thread::sleep(20);
    }
    auto img = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
    juce::File f(argc > 1 ? argv[1] : "editor.png");
    f.deleteFile();
    juce::FileOutputStream out(f);
    juce::PNGImageFormat().writeImageToStream(img, out);
    {   // the mixer on its own
        MixerView mixer(*g);
        mixer.setSize(420, 230);
        auto m = mixer.createComponentSnapshot(mixer.getLocalBounds(), true, 1.0f);
        juce::File mf(f.getSiblingFile(f.getFileNameWithoutExtension() + "_mixer.png"));
        mf.deleteFile();
        juce::FileOutputStream mo(mf);
        juce::PNGImageFormat().writeImageToStream(m, mo);
    }
    plugin->editorBeingDeleted(editor.get());
    editor.reset();
    plugin->releaseResources();
    return 0;
}
