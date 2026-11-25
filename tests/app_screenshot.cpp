// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The screenshot of the documentation: the app as it opens (standalone, its demo song), playing. The editor is in a
// window of its own so that its views refresh as they do in the app (VBlankAttachment), the audio runs in real time
// on a thread of its own, and the editor draws itself into the picture (nothing is read from the screen).
// Usage: app_screenshot out.png [seconds of playing]
#include <atomic>
#include <chrono>
#include <thread>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../plugin/PluginEditor.h"
#include "../plugin/PluginProcessor.h"

int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    // the standalone's interface: play, tempo and full screen in the transport bar
    juce::PluginHostType::jucePlugInClientCurrentWrapperType = juce::AudioProcessor::wrapperType_Standalone;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    std::unique_ptr<juce::AudioProcessor> plugin(createPluginFilter());
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    const double rate = 48000.0;
    const int block = 512;
    plugin->setPlayConfigDetails(0, 2, rate, block);
    plugin->prepareToPlay(rate, block);
    auto &processor = dynamic_cast<GRMGuitarProcessor &>(*plugin);

    // the audio thread, in real time
    std::atomic<bool> running{true};
    std::thread audio([&]
                      {
        juce::AudioBuffer<float> buffer(2, block);
        juce::MidiBuffer midi;
        const auto period = std::chrono::microseconds((long long)(1e6 * block / rate));
        auto next = std::chrono::steady_clock::now();
        while (running.load())
        {
            buffer.clear();
            plugin->processBlock(buffer, midi);
            next += period;
            std::this_thread::sleep_until(next);
        } });

    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());
    editor->setSize(1600, 880);
    editor->addToDesktop(juce::ComponentPeer::windowHasTitleBar);
    editor->setVisible(true);
    // the message loop runs as in the app; the steps are scheduled on it: play once the networks are solved and the
    // views filled, then take the picture after some seconds of playing
    auto *messages = juce::MessageManager::getInstance();
    const int seconds = argc > 2 ? juce::String(argv[2]).getIntValue() : 6;
    const juce::String path(argc > 1 ? argv[1] : "screenshot.png");
    juce::Timer::callAfterDelay(4000, [&]
                                { processor.setInternalPlaying(true); });
    juce::Timer::callAfterDelay(4000 + 1000 * seconds, [&]
                                {
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
        messages->stopDispatchLoop(); });
    messages->runDispatchLoop();

    processor.setInternalPlaying(false);
    running.store(false);
    audio.join();
    plugin->editorBeingDeleted(editor.get());
    editor.reset();
    plugin->releaseResources();
    return 0;
}
