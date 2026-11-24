// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

#include <cstdint>

namespace ui::theme
{
    struct Palette
    {
        std::uint32_t obsidian = 0xff0D0D0E;
        std::uint32_t charcoalDark = 0xff141416;
        std::uint32_t charcoal = 0xff1C1C1E;
        std::uint32_t charcoalLight = 0xff28282B;
        std::uint32_t warmWhite = 0xffE8E6E1;
        std::uint32_t mediumGrey = 0xff9A9A9E;
        std::uint32_t darkGrey = 0xff55555A;
        std::uint32_t steelGrey = 0xff9A9A9A;
        std::uint32_t coolGrey = 0xff7A8590;
        std::uint32_t terracotta = 0xffD96850;
        std::uint32_t terracottaLight = 0xffE07060;
        std::uint32_t terracottaPale = 0xffEB8777;
        std::uint32_t terracottaDark = 0xffA04840;
        std::uint32_t sageGreen = 0xff6BB38A;
        std::uint32_t amber = 0xffE8A860;
        std::uint32_t amberWarm = 0xffE8A35E;
        std::uint32_t gold = 0xffD9A54E;
        std::uint32_t mauve = 0xff8B6AB5;
        std::uint32_t teal = 0xff4DA3B3;
        std::uint32_t cyan = 0xff4DC4D4;
        std::uint32_t emerald = 0xff4FA88C;
        std::uint32_t slateBlue = 0xff6B8299;
        std::uint32_t indigo = 0xff5568A0;
        std::uint32_t rose = 0xffCB7AA8;
        std::uint32_t lime = 0xff8BBF5A;
        std::uint32_t mutedSteel = 0xff5A6770;
        std::uint32_t mossGreen = 0xff7B8A5E;
        std::uint32_t deepSlate = 0xff2D3640;
    };

    struct Opacity
    {
        float alpha01 = 0.1f;
        float alpha02 = 0.2f;
        float alpha03 = 0.3f;
        float alpha04 = 0.4f;
        float alpha06 = 0.6f;
        float alpha08 = 0.8f;
        float alpha09 = 0.9f;
    };

    struct ColorScheme
    {
        Palette palette;

        static ColorScheme dark()
        {
            return ColorScheme{};
        }
    };

    inline constexpr Palette defaultPalette{};
    inline constexpr Opacity defaultOpacity{};
}