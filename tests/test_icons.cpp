// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The icons of the interface: every icon has its file, the file is embedded and holds a shape that fits its
// canvas, drawing it puts ink on the canvas, and every icon button of the editor tells what it does (tooltip)
// at the size of the design system.
#include <array>
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"
#include "../plugin/PluginEditor.h"
#include "../plugin/style/IconLibrary.h"
#include "../plugin/views/MixerView.h"

using ui::IconId;

static int failures = 0;

static void check(bool ok, const juce::String &what)
{
    if (!ok)
    {
        ++failures;
        std::printf("FAIL  %s\n", what.toRawUTF8());
    }
}

static void everyIconHasOneFile()
{
    std::array<int, ui::ICON_COUNT> seen{};
    for (const auto &entry : ui::ICON_FILES)
        ++seen[static_cast<std::size_t>(entry.id)];
    for (std::size_t k = 0; k < seen.size(); ++k)
        check(seen[k] == 1, "icon " + juce::String((int)k) + " is listed " + juce::String(seen[k]) + " times in ICON_FILES");
}

static void everyIconLoadsAndDraws()
{
    const auto &library = IconLibrary::getInstance();
    for (const auto &entry : ui::ICON_FILES)
    {
        const juce::String name(entry.file);
        check(library.isLoaded(entry.id), name + " is missing from the binary data or holds no shape");

        juce::Image image(juce::Image::ARGB, 32, 32, true);
        {
            juce::Graphics g(image);
            library.draw(g, entry.id, {0.0f, 0.0f, 32.0f, 32.0f}, juce::Colours::white);
        }
        int inked = 0;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                inked += image.getPixelAt(x, y).getAlpha() > 0 ? 1 : 0;
        check(inked > 8, name + " draws nothing");
        check(inked < 32 * 32, name + " fills its whole square (canvas not read)");
    }
}

static void collectIconButtons(juce::Component &parent, std::vector<IconButton *> &found)
{
    for (auto *child : parent.getChildren())
    {
        if (auto *button = dynamic_cast<IconButton *>(child))
            found.push_back(button);
        collectIconButtons(*child, found);
    }
}

// every icon button inside `root` tells what it does and, once laid out, has the size of the design system
static void checkIconButtons(juce::Component &root, std::size_t expected, const juce::String &where)
{
    std::vector<IconButton *> buttons;
    collectIconButtons(root, buttons);
    check(buttons.size() == expected,
          where + ": " + juce::String((int)buttons.size()) + " icon buttons, " + juce::String((int)expected) + " expected");

    const int size = ui::theme::AppTheme::fromScale(1.0f).dimensions.iconButton;
    for (auto *button : buttons)
    {
        check(button->getTooltip().isNotEmpty(), where + ": an icon button has no tooltip");
        if (button->isVisible() && button->getWidth() > 0)
            check(button->getWidth() == size && button->getHeight() == size,
                  where + ": \"" + button->getTooltip() + "\" is " + juce::String(button->getWidth()) + "x" +
                      juce::String(button->getHeight()));
    }
}

static void interfaceIconButtonsAreDescribedAndSquare()
{
    std::unique_ptr<juce::AudioProcessor> plugin(createPluginFilter());
    plugin->setPlayConfigDetails(0, 2, 48000.0, 512);
    plugin->prepareToPlay(48000.0, 512);
    auto &processor = dynamic_cast<GRMGuitarProcessor &>(*plugin);
    processor.addGuitar();
    {
        // take and export 2, the transport 4 (play, loop, follow, full screen), roll tools 6, patterns 5, guitars 2, the
        // ghosts 1, the Feel's 3, the mixer's mute and solo of the two guitars (with the presets' save and delete), at
        // the editor's smallest size
        std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());
        const auto theme = ui::theme::AppTheme::fromScale(1.0f);
        editor->setSize(theme.dimensions.editorMinWidth, theme.dimensions.editorMinHeight);
        checkIconButtons(*editor, 29, "editor");

        // mute and solo on each guitar's strip
        MixerView mixer(processor);
        mixer.setSize(400, 300);
        checkIconButtons(mixer, 2 * (std::size_t)processor.getGuitarCount(), "mixer");

        // the mixer follows the guitars (Anthony, 25/11/2026: a guitar added, the mixer kept its size): as wide as its
        // strips, in the container that scrolls through them
        const int strip = theme.dimensions.mixerStripWidth;
        processor.addGuitar();
        processor.addGuitar();
        mixer.refresh();
        check(mixer.getWidth() == 4 * strip, "mixer: four guitars, four strips wide");
        processor.removeGuitar(0);
        mixer.refresh();
        check(mixer.getWidth() == 3 * strip, "mixer: a guitar removed, a strip narrower");
    }
    plugin->releaseResources();
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    everyIconHasOneFile();
    everyIconLoadsAndDraws();
    interfaceIconButtonsAreDescribedAndSquare();
    if (failures == 0)
        std::printf("icons: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
