// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// Types shared by the interface.
#pragma once

#include <cstddef>

namespace ui
{
    // Every icon of the interface, named after what it draws (not after the button using it), so that one
    // glyph can serve several buttons.
    enum class IconId
    {
        play,
        stop,
        arrowsClockwise,
        exportFile,
        strum,
        arpeggio,
        eraser,
        arrowCounterClockwise,
        arrowClockwise,
        musicNotes,
        filePlus,
        copySimple,
        copy,
        clipboard,
        trash,
        plus,
        minus,
        ghost,
        crosshair,
        speakerSlash,
        headphones,
        caretUp,
        caretDown,
        caretLeft,
        caretRight,
        floppyDisk,
        count
    };

    inline constexpr std::size_t ICON_COUNT = static_cast<std::size_t>(IconId::count);

    // An icon and the SVG file (in assets/icons, embedded as binary data) it is drawn from.
    struct IconFile
    {
        IconId id;
        const char *file;
    };
}
