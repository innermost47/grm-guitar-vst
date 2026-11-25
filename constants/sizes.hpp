// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

namespace ui::theme
{
    struct Spacing
    {
        int xxs = 1;
        int xs = 2;
        int sm = 3;
        int base = 4;
        int md = 6;
        int lg = 8;
        int xl = 12;
        int padding = 6;
    };

    struct Borders
    {
        float xs = 0.5f;
        float sm = 0.6f;
        float base = 1.0f;
        float xl = 1.2f;
        int corner = 1;
        float halfCorner = 0.5f;
        float listPanelCorner = 1.0f;
        float modalCorner = 6.0f;
    };

    struct Typography
    {
        float xxs = 8.0f;
        float xs = 10.0f;
        float sm = 11.0f;
        float small = 12.0f;
        float info = 13.0f;
        float regular = 14.0f;
        float subtitle = 16.0f;
        float title = 16.0f;
        float xl = 18.0f;
        float xxl = 22.0f;
        float splash = 26.0f;
        float mixerKnobLabel = 9.0f;
        float sendKnobLabel = 12.0f;
        float mixerLabelName = 13.0f;
    };

    struct Dimensions
    {
        int baseModalWidth = 600;
        int baseModalHeight = 400;
        int modalSideWidth = 150; // the column of the equations on the left of every dialog
        int modalMinHeight = 300; // a dialog grows with its text from there (up to baseModalHeight)
        int modalPadding = 18;
        int modalTitleHeight = 30;
        int modalRowHeight = 24; // a checkbox, a link
        int modalButtonWidth = 110;
        int modalButtonHeight = 28;
        int presetBarWidth = 320;   // the presets' menu and its two buttons
        int tensionViewHeight = 66; // the strings' notes and tensions (Strings tab), on two lines when narrow
        // the editor: its size, the settings' sidebar (a share of the width), its column of tabs, the presets' row,
        // a mixer strip and its meter, a Feel lane under the roll
        int editorWidth = 1600;
        int editorHeight = 820;
        int editorMinWidth = 1480; // the roll's two bars fit beside the sidebar
        int editorMinHeight = 760;
        int sidebarPercent = 25;
        int sideTabWidth = 108;
        int sideTabHeight = 30;
        int presetRowHeight = 34;
        int mixerStripWidth = 120;
        int vuMeterWidth = 16;
        int feelLaneHeight = 34;
        // the top line (presets, transport, title), the networks' figures under the settings, the mixer's share of the
        // height above the roll, the neck's share of the width beside the mixer and its height
        int topLineHeight = 44;
        int titleWidth = 300;
        int networksHeight = 200;
        int mixerPercent = 34;
        int neckPercent = 42;
        int neckHeight = 160;
        int minSmallBtnWidth = 24;
        int minSmallBtnHeight = 24;
        int iconButton = 24;
        int iconButtonPadding = 5;
        int sampleDetailHeightVst = 122;
        int sampleDetailHeightStandalone = 94;
        int sampleRowHeight = 64;
        int mixerChannelKnob = 32;
        int comboBoxBaseHeight = 20;
        int trackBaseHeight = 80;
        int waveformHeight = 45;
        int sequencerHeight = 45;
        int pageButtonSize = 16;
        int tabBarHeight = 20;
        int titlePanelHeight = 28;
        int infoPanelHeight = 32;
        int scaleAndDurationHeight = 114;
        int configAreaHeight = 138;
        int gateHeight = 70;
        int distortionHeight = 82;
        int bitcrusherHeight = 70;
        int eqHeight = 130;
        int filterHeight = 80;
        int chorusHeight = 70;
        int flangerHeight = 70;
        int phaserHeight = 70;
        int compressorHeight = 70;
        int limiterHeight = 70;
    };

    struct AppTheme
    {
        float scale = 1.0f;
        Spacing spacing;
        Borders borders;
        Typography typography;
        Dimensions dimensions;

        static AppTheme fromScale(float uiScale)
        {
            AppTheme theme;
            theme.scale = uiScale;
            theme.spacing.xxs = static_cast<int>(1 * uiScale);
            theme.spacing.xs = static_cast<int>(2 * uiScale);
            theme.spacing.sm = static_cast<int>(3 * uiScale);
            theme.spacing.base = static_cast<int>(4 * uiScale);
            theme.spacing.md = static_cast<int>(6 * uiScale);
            theme.spacing.lg = static_cast<int>(8 * uiScale);
            theme.spacing.xl = static_cast<int>(12 * uiScale);
            theme.spacing.padding = static_cast<int>(6 * uiScale);

            theme.borders.xs = 0.5f * uiScale;
            theme.borders.sm = 0.6f * uiScale;
            theme.borders.base = 1.0f * uiScale;
            theme.borders.xl = 1.2f * uiScale;
            theme.borders.corner = static_cast<int>(1 * uiScale);
            theme.borders.halfCorner = 0.5f * uiScale;
            theme.borders.listPanelCorner = 1.0f * uiScale;
            theme.borders.modalCorner = 6.0f * uiScale;

            theme.typography.xxs = 8.0f * uiScale;
            theme.typography.xs = 10.0f * uiScale;
            theme.typography.sm = 11.0f * uiScale;
            theme.typography.small = 12.0f * uiScale;
            theme.typography.info = 13.0f * uiScale;
            theme.typography.regular = 14.0f * uiScale;
            theme.typography.subtitle = 16.0f * uiScale;
            theme.typography.title = 16.0f * uiScale;
            theme.typography.xl = 18.0f * uiScale;
            theme.typography.xxl = 22.0f * uiScale;
            theme.typography.splash = 26.0f * uiScale;
            theme.typography.mixerKnobLabel = 9.0f * uiScale;
            theme.typography.sendKnobLabel = 12.0f * uiScale;
            theme.typography.mixerLabelName = 13.0f * uiScale;

            theme.dimensions.baseModalWidth = static_cast<int>(600 * uiScale);
            theme.dimensions.baseModalHeight = static_cast<int>(400 * uiScale);
            theme.dimensions.modalSideWidth = static_cast<int>(150 * uiScale);
            theme.dimensions.modalMinHeight = static_cast<int>(300 * uiScale);
            theme.dimensions.modalPadding = static_cast<int>(18 * uiScale);
            theme.dimensions.modalTitleHeight = static_cast<int>(30 * uiScale);
            theme.dimensions.modalRowHeight = static_cast<int>(24 * uiScale);
            theme.dimensions.modalButtonWidth = static_cast<int>(110 * uiScale);
            theme.dimensions.modalButtonHeight = static_cast<int>(28 * uiScale);
            theme.dimensions.presetBarWidth = static_cast<int>(320 * uiScale);
            theme.dimensions.tensionViewHeight = static_cast<int>(66 * uiScale);
            theme.dimensions.editorWidth = static_cast<int>(1600 * uiScale);
            theme.dimensions.editorHeight = static_cast<int>(820 * uiScale);
            theme.dimensions.editorMinWidth = static_cast<int>(1480 * uiScale);
            theme.dimensions.editorMinHeight = static_cast<int>(760 * uiScale);
            theme.dimensions.sideTabWidth = static_cast<int>(108 * uiScale);
            theme.dimensions.sideTabHeight = static_cast<int>(30 * uiScale);
            theme.dimensions.presetRowHeight = static_cast<int>(34 * uiScale);
            theme.dimensions.mixerStripWidth = static_cast<int>(120 * uiScale);
            theme.dimensions.vuMeterWidth = static_cast<int>(16 * uiScale);
            theme.dimensions.feelLaneHeight = static_cast<int>(34 * uiScale);
            theme.dimensions.topLineHeight = static_cast<int>(44 * uiScale);
            theme.dimensions.titleWidth = static_cast<int>(300 * uiScale);
            theme.dimensions.networksHeight = static_cast<int>(200 * uiScale);
            theme.dimensions.neckHeight = static_cast<int>(160 * uiScale);
            theme.dimensions.minSmallBtnWidth = static_cast<int>(24 * uiScale);
            theme.dimensions.minSmallBtnHeight = static_cast<int>(24 * uiScale);
            theme.dimensions.iconButton = static_cast<int>(24 * uiScale);
            theme.dimensions.iconButtonPadding = static_cast<int>(5 * uiScale);
            theme.dimensions.sampleDetailHeightVst = static_cast<int>(122 * uiScale);
            theme.dimensions.sampleDetailHeightStandalone = static_cast<int>(94 * uiScale);
            theme.dimensions.sampleRowHeight = static_cast<int>(64 * uiScale);
            theme.dimensions.mixerChannelKnob = static_cast<int>(32 * uiScale);
            theme.dimensions.comboBoxBaseHeight = static_cast<int>(20 * uiScale);
            theme.dimensions.trackBaseHeight = static_cast<int>(80 * uiScale);
            theme.dimensions.waveformHeight = static_cast<int>(45 * uiScale);
            theme.dimensions.sequencerHeight = static_cast<int>(45 * uiScale);
            theme.dimensions.pageButtonSize = static_cast<int>(16 * uiScale);
            theme.dimensions.tabBarHeight = static_cast<int>(20 * uiScale);
            theme.dimensions.titlePanelHeight = static_cast<int>(28 * uiScale);
            theme.dimensions.infoPanelHeight = static_cast<int>(32 * uiScale);
            theme.dimensions.scaleAndDurationHeight = static_cast<int>(114 * uiScale);
            theme.dimensions.configAreaHeight = static_cast<int>(138 * uiScale);
            theme.dimensions.gateHeight = static_cast<int>(70 * uiScale);
            theme.dimensions.distortionHeight = static_cast<int>(82 * uiScale);
            theme.dimensions.bitcrusherHeight = static_cast<int>(70 * uiScale);
            theme.dimensions.eqHeight = static_cast<int>(130 * uiScale);
            theme.dimensions.filterHeight = static_cast<int>(80 * uiScale);
            theme.dimensions.chorusHeight = static_cast<int>(70 * uiScale);
            theme.dimensions.flangerHeight = static_cast<int>(70 * uiScale);
            theme.dimensions.phaserHeight = static_cast<int>(70 * uiScale);
            theme.dimensions.compressorHeight = static_cast<int>(70 * uiScale);
            theme.dimensions.limiterHeight = static_cast<int>(70 * uiScale);

            return theme;
        }
    };
}