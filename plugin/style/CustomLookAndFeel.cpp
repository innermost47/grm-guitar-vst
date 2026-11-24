#include "CustomLookAndFeel.h"
#include "BinaryData.h"

using namespace ui;

CustomLookAndFeel::CustomLookAndFeel()
{
    const ui::theme::Palette palette{};

    const juce::Colour backgroundDeep{palette.obsidian};
    const juce::Colour backgroundDark{palette.charcoalDark};
    const juce::Colour backgroundLight{palette.charcoalLight};
    const juce::Colour textPrimary{palette.warmWhite};
    const juce::Colour lightGrey{palette.mediumGrey};
    const juce::Colour buttonPrimary{palette.cyan};
    const juce::Colour buttonSuccess{palette.sageGreen};
    const juce::Colour muteActive{palette.terracotta};

    setColour(juce::TextButton::buttonColourId, soften(backgroundLight));
    setColour(juce::TextButton::buttonOnColourId, soften(buttonSuccess));
    setColour(juce::TextButton::textColourOffId, textPrimary);
    setColour(juce::TextButton::textColourOnId, textPrimary);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, textPrimary);
    setColour(juce::ToggleButton::tickColourId, soften(buttonSuccess));
    setColour(juce::TextEditor::backgroundColourId, backgroundDark);
    setColour(juce::TextEditor::textColourId, textPrimary);
    setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::focusedOutlineColourId, buttonPrimary.withAlpha(0.6f));
    setColour(juce::TextEditor::highlightColourId, buttonPrimary.withAlpha(0.3f));
    setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    setColour(juce::ScrollBar::thumbColourId, muteActive);
    setColour(juce::ScrollBar::backgroundColourId, backgroundDeep);
    setColour(juce::ComboBox::backgroundColourId, backgroundDark);
    setColour(juce::PopupMenu::backgroundColourId, backgroundDark);
    setColour(juce::PopupMenu::textColourId, textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, buttonPrimary.withAlpha(0.4f));
    setColour(juce::PopupMenu::highlightedTextColourId, textPrimary);
    setColour(juce::TextEditor::focusedOutlineColourId, lightGrey);
    setColour(juce::TextEditor::highlightColourId, lightGrey.withAlpha(0.3f));
    setColour(juce::TextEditor::highlightedTextColourId, textPrimary);
    setColour(juce::CaretComponent::caretColourId, lightGrey);
}

juce::Typeface::Ptr CustomLookAndFeel::getTypefaceForFont(const juce::Font &font)
{
    const auto requestedName = font.getTypefaceName();

    if (fontFamily.michroma != nullptr && requestedName == fontFamily.michroma->getName())
    {
        return fontFamily.michroma;
    }

    if (font.isBold() && fontFamily.notoBold != nullptr)
    {
        return fontFamily.notoBold;
    }

    if (font.isItalic() && fontFamily.notoItalic != nullptr)
    {
        return fontFamily.notoItalic;
    }

    return fontFamily.notoRegular;
}

juce::Colour CustomLookAndFeel::soften(const juce::Colour &colour)
{
    return colour.withSaturation(colour.getSaturation() * 0.85f).brighter(0.05f);
}

juce::TextLayout CustomLookAndFeel::layoutTooltipText(const juce::String &text, juce::Colour colour)
{
    const float tooltipFontSize = 12.0f;
    const int maxToolTipWidth = 400;

    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);

    s.append(text, juce::Font(juce::FontOptions(theme::FontFamily::load().notoRegular).withHeight(tooltipFontSize)), colour);

    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, (float)maxToolTipWidth);
    return tl;
}

juce::Rectangle<int> CustomLookAndFeel::getTooltipBounds(const juce::String &tipText, juce::Point<int> screenPos,
                                                         juce::Rectangle<int> parentArea)
{
    const juce::TextLayout tl(layoutTooltipText(tipText, juce::Colour(palette.warmWhite)));
    auto w = (int)(tl.getWidth() + 16.0f);
    auto h = (int)(tl.getHeight() + 10.0f);
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin(parentArea);
}

void CustomLookAndFeel::drawTooltip(juce::Graphics &graphics, const juce::String &text, int width, int height)
{
    const float corner = static_cast<float>(appTheme.borders.corner);
    juce::Rectangle<float> bounds(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);

    graphics.setColour(juce::Colour(palette.charcoalDark));
    graphics.fillRoundedRectangle(bounds, corner);

    graphics.setColour(juce::Colour(palette.mediumGrey).withAlpha(0.5f));
    graphics.drawRoundedRectangle(bounds, corner, appTheme.borders.base);

    layoutTooltipText(text, juce::Colour(palette.warmWhite))
        .draw(graphics, bounds.reduced(static_cast<float>(appTheme.spacing.lg), static_cast<float>(appTheme.spacing.base)));
}

void CustomLookAndFeel::drawButtonBackground(juce::Graphics &g, juce::Button &button,
                                             const juce::Colour &backgroundColour, bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto baseColour = soften(backgroundColour).withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);

    if (shouldDrawButtonAsDown)
        baseColour = baseColour.darker(0.15f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter(0.12f);

    if (!shouldDrawButtonAsDown)
    {
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.translated(0, 1.5f), appTheme.borders.corner);
    }

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, appTheme.borders.corner);

    g.setColour(baseColour.brighter(0.2f).withAlpha(0.4f));
    g.drawRoundedRectangle(bounds, appTheme.borders.corner, appTheme.borders.sm);
}

void CustomLookAndFeel::drawButtonText(juce::Graphics &g, juce::TextButton &button,
                                       bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    auto textColour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                : juce::TextButton::textColourOffId);

    if (!button.isEnabled())
        textColour = textColour.withAlpha(0.5f);

    g.setColour(textColour);
    g.setFont(juce::Font(juce::FontOptions(theme::FontFamily::load().notoBold).withHeight(appTheme.typography.regular)));

    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 2, 0.8f);
}

void CustomLookAndFeel::drawToggleButton(juce::Graphics &g, juce::ToggleButton &button,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();

    auto bgColour = button.findColour(juce::TextButton::buttonColourId);

    if (!button.isEnabled())
        bgColour = bgColour.withAlpha(0.5f);
    else if (shouldDrawButtonAsDown)
        bgColour = bgColour.darker(0.15f);
    else if (shouldDrawButtonAsHighlighted)
        bgColour = bgColour.brighter(0.12f);

    if (!shouldDrawButtonAsDown)
    {
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.translated(0, 1.5f), appTheme.borders.corner);
    }

    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds, appTheme.borders.corner);

    if (!shouldDrawButtonAsDown)
    {
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        auto topBounds = bounds.withHeight(bounds.getHeight() * 0.4f);
        g.fillRoundedRectangle(topBounds, appTheme.borders.corner);
    }

    g.setColour(bgColour.brighter(0.2f).withAlpha(0.4f));
    g.drawRoundedRectangle(bounds, appTheme.borders.corner, appTheme.borders.base);

    auto textColour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                : juce::TextButton::textColourOffId);

    if (!button.isEnabled())
        textColour = textColour.withAlpha(0.5f);

    g.setColour(textColour);
    g.setFont(juce::Font(juce::FontOptions(theme::FontFamily::load().notoRegular).withHeight(appTheme.typography.regular)));
    g.drawText(button.getButtonText(), bounds, juce::Justification::centred);
}

juce::AlertWindow *CustomLookAndFeel::createAlertWindow(const juce::String &title, const juce::String &message,
                                                        const juce::String & /*button1*/,
                                                        const juce::String & /*button2*/,
                                                        const juce::String & /*button3*/,
                                                        juce::MessageBoxIconType iconType, int /*numButtons*/,
                                                        juce::Component *associatedComponent)
{
    auto *aw = new juce::AlertWindow(title, message, iconType, associatedComponent);

    aw->setColour(juce::AlertWindow::backgroundColourId, juce::Colour(palette.charcoalDark));
    aw->setColour(juce::AlertWindow::textColourId, juce::Colour(palette.warmWhite));
    aw->setColour(juce::AlertWindow::outlineColourId, juce::Colour(palette.terracotta).withAlpha(0.6f));

    return aw;
}

void CustomLookAndFeel::drawAlertBox(juce::Graphics &g, juce::AlertWindow &alert, const juce::Rectangle<int> &textArea,
                                     juce::TextLayout &textLayout)
{
    g.fillAll(juce::Colour(palette.charcoalDark));

    g.setColour(juce::Colour(palette.terracotta).withAlpha(0.5f));
    g.drawRoundedRectangle(alert.getLocalBounds().toFloat().reduced(1.0f), appTheme.borders.corner, 1.5f);

    auto titleBar = alert.getLocalBounds().removeFromTop(42).toFloat();
    g.setColour(juce::Colour(palette.terracotta).withAlpha(0.2f));
    g.fillRect(titleBar);

    g.setColour(juce::Colour(palette.terracotta).withAlpha(0.5f));
    g.drawLine(titleBar.getBottomLeft().x, titleBar.getBottom(), titleBar.getBottomRight().x, titleBar.getBottom(),
               1.0f);

    textLayout.draw(g, textArea.toFloat());
}

juce::Font CustomLookAndFeel::getAlertWindowTitleFont()
{
    return juce::Font(juce::FontOptions(theme::FontFamily::load().michroma).withHeight(appTheme.typography.subtitle));
}

juce::Font CustomLookAndFeel::getAlertWindowMessageFont()
{
    return juce::Font(juce::FontOptions(theme::FontFamily::load().notoBold).withHeight(appTheme.typography.regular));
}

juce::Font CustomLookAndFeel::getAlertWindowFont()
{
    return juce::Font(juce::FontOptions(theme::FontFamily::load().notoRegular).withHeight(appTheme.typography.regular));
}

void CustomLookAndFeel::drawComboBox(juce::Graphics &g, int width, int height, bool isButtonDown, int buttonX,
                                     int buttonY, int buttonW, int buttonH, juce::ComboBox &box)
{
    auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f);
    const float corner = appTheme.borders.corner;

    if (!isButtonDown)
    {
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(bounds.translated(0, 1.0f), corner);
    }

    g.setColour(juce::Colour(palette.charcoal));
    g.fillRoundedRectangle(bounds, corner);

    auto borderColour = juce::Colour(palette.steelGrey).withAlpha(opacity.alpha04);
    float borderThickness = appTheme.borders.sm;

    if (box.hasKeyboardFocus(false))
    {
        borderColour = juce::Colour(palette.steelGrey).withAlpha(opacity.alpha08);
        borderThickness = appTheme.borders.xl;
    }
    else if (box.isMouseOver())
    {
        borderColour = juce::Colour(palette.steelGrey).withAlpha(opacity.alpha06);
        borderThickness = appTheme.borders.base;
    }

    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds, corner, borderThickness);

    auto arrowZone = juce::Rectangle<float>((float)buttonX, (float)buttonY, (float)buttonW, (float)buttonH);

    auto chevronDownSvg = juce::Drawable::createFromImageData(BinaryData::caretdown_svg, BinaryData::caretdown_svgSize);
    if (chevronDownSvg != nullptr)
    {
        chevronDownSvg->replaceColour(juce::Colours::black, juce::Colour(palette.warmWhite));
        chevronDownSvg->drawWithin(g, arrowZone.toFloat().reduced(4), juce::RectanglePlacement::centred,
                                   opacity.alpha04);
    }
}

juce::Font CustomLookAndFeel::getComboBoxFont(juce::ComboBox & /*box*/)
{
    auto fontSize = appTheme.typography.regular;
    return juce::Font(juce::FontOptions(theme::FontFamily::load().notoRegular).withHeight(fontSize));
}

void CustomLookAndFeel::positionComboBoxText(juce::ComboBox &box, juce::Label &label)
{
    auto buttonWidth = juce::jmin(box.getHeight(), 24);

    label.setBounds(0, 0, box.getWidth() - buttonWidth, box.getHeight());

    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::left);
    label.setColour(juce::Label::textColourId, juce::Colour(palette.warmWhite));
    label.setBorderSize(juce::BorderSize<int>(0, 0, 0, 0));
}

juce::PopupMenu::Options CustomLookAndFeel::getOptionsForComboBoxPopupMenu(juce::ComboBox &box, juce::Label &label)
{
    return juce::PopupMenu::Options()
        .withTargetComponent(&box)
        .withItemThatMustBeVisible(box.getSelectedId())
        .withInitiallySelectedItem(box.getSelectedId())
        .withMinimumWidth(box.getWidth())
        .withMaximumNumColumns(1)
        .withStandardItemHeight(juce::jmax(24, label.getHeight()));
}

void CustomLookAndFeel::drawLabel(juce::Graphics &g, juce::Label &label)
{
    bool isSliderTextBox = (dynamic_cast<juce::Slider *>(label.getParentComponent()) != nullptr);

    float currentSize = label.getFont().getHeight();
    auto currentFont = label.getFont();
    auto fontName = currentFont.getTypefaceName();

    if (fontName == theme::FontFamily::load().michroma->getName())
    {
        g.setFont(juce::FontOptions(theme::FontFamily::load().michroma).withHeight(currentSize));
    }
    else
    {
        if (label.getFont().isBold())
        {
            g.setFont(juce::FontOptions(theme::FontFamily::load().notoBold).withHeight(currentSize));
        }
        else if (label.getFont().isItalic())
        {
            g.setFont(juce::FontOptions(theme::FontFamily::load().notoItalic).withHeight(currentSize));
        }
        else
        {
            g.setFont(juce::FontOptions(theme::FontFamily::load().notoRegular).withHeight(currentSize));
        }
    }

    if (isSliderTextBox)
    {
        auto bounds = label.getLocalBounds().toFloat();
        g.setColour(juce::Colour(palette.charcoalDark));
        g.fillRoundedRectangle(bounds, appTheme.borders.corner);
        g.setColour(juce::Colour(palette.steelGrey).withAlpha(0.4f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), appTheme.borders.corner, 0.8f);

        if (!label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
            auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
            g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                             juce::jmax(1, (int)((float)textArea.getHeight() / label.getFont().getHeight())), 1.0f);
        }
        return;
    }

    g.fillAll(label.findColour(juce::Label::backgroundColourId));
    if (!label.isBeingEdited())
    {
        auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        auto textColour = label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha);
        g.setColour(textColour);
        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());

        float fontHeight = label.getFont().getHeight();
        if (textArea.isEmpty() || fontHeight <= 0.0f)
            return;

        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, (int)((float)textArea.getHeight() / fontHeight)), 1.0f);
    }
    else if (label.isEnabled())
    {
        g.setColour(label.findColour(juce::Label::outlineColourId));
    }
}

juce::BorderSize<int> CustomLookAndFeel::getLabelBorderSize(juce::Label & /*label*/)
{
    return juce::BorderSize<int>(1, 5, 1, 5);
}

const juce::Identifier &CustomLookAndFeel::getDrawTicksPropertyId()
{
    static const juce::Identifier id("drawTicks");
    return id;
}

const juce::Identifier &CustomLookAndFeel::getDrawTicksSmallPropertyId()
{
    static const juce::Identifier id("drawTicksSmall");
    return id;
}

void CustomLookAndFeel::drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height, float sliderPos,
                                         float /*minSliderPos*/, float /*maxSliderPos*/,
                                         const juce::Slider::SliderStyle style, juce::Slider &slider)
{
    auto accentColour = slider.isColourSpecified(juce::Slider::thumbColourId)
                            ? slider.findColour(juce::Slider::thumbColourId)
                            : juce::Colour(palette.terracotta);

    if (style == juce::Slider::LinearVertical || style == juce::Slider::LinearHorizontal)
    {
        const bool isVertical = (style == juce::Slider::LinearVertical);
        auto trackWidth = juce::jmin(5.0f, (float)(isVertical ? width : height) * 0.20f);

        juce::Point<float> startPoint, endPoint;
        if (isVertical)
        {
            auto cx = (float)x + (float)width * 0.5f;
            startPoint = {cx, (float)y};
            endPoint = {cx, (float)(y + height)};
        }
        else
        {
            auto cy = (float)y + (float)height * 0.5f;
            startPoint = {(float)x, cy};
            endPoint = {(float)(x + width), cy};
        }

        auto trackRect = juce::Rectangle<float>(startPoint, endPoint).expanded(trackWidth * 0.5f);

        const auto &props = slider.getProperties();
        int numTicks = props[getDrawTicksPropertyId()];
        if (numTicks >= 2)
        {
            const bool small = (bool)props[getDrawTicksSmallPropertyId()];
            drawGraduationTicks(g, trackRect, numTicks, isVertical, small);
        }

        g.setColour(juce::Colour(palette.charcoal));
        g.fillRoundedRectangle(trackRect, trackWidth * 0.5f);

        if (isVertical)
        {
            float thumbY = sliderPos;
            float bottomY = (float)(y + height);
            float cx = (float)x + (float)width * 0.5f;

            auto fillRect = juce::Rectangle<float>(cx - trackWidth * 0.5f, thumbY, trackWidth, bottomY - thumbY);

            if (fillRect.getHeight() > 0.5f)
            {
                g.setColour(accentColour.withAlpha(0.5f));
                g.fillRoundedRectangle(fillRect, trackWidth * 0.5f);
            }
        }

        g.setColour(juce::Colour(palette.charcoalLight).withAlpha(0.5f));
        g.drawRoundedRectangle(trackRect, trackWidth * 0.5f, 0.6f);

        float capsuleW, capsuleH;
        if (isVertical)
        {
            capsuleW = 20.0f;
            capsuleH = 12.0f;
        }
        else
        {
            capsuleW = 22.0f;
            capsuleH = juce::jmax(20.0f, juce::jmin(30.0f, (float)height * 0.75f));
        }
        const float capsuleR = appTheme.borders.corner * 2;

        juce::Rectangle<float> capsule;
        if (isVertical)
            capsule =
                juce::Rectangle<float>(capsuleW, capsuleH).withCentre({(float)x + (float)width * 0.5f, sliderPos});
        else
            capsule =
                juce::Rectangle<float>(capsuleW, capsuleH).withCentre({sliderPos, (float)y + (float)height * 0.5f});

        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(capsule.translated(0, 2.0f), capsuleR);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(capsule.translated(0, 1.0f), capsuleR);

        juce::ColourGradient bodyGradient(juce::Colour(palette.charcoalLight).brighter(0.15f), capsule.getX(),
                                          capsule.getY(), juce::Colour(palette.charcoal).darker(0.2f), capsule.getX(),
                                          capsule.getBottom(), false);
        bodyGradient.addColour(0.5, juce::Colour(palette.charcoalLight).darker(0.05f));
        g.setGradientFill(bodyGradient);
        g.fillRoundedRectangle(capsule, capsuleR);

        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillRoundedRectangle(capsule.withHeight(capsule.getHeight() * 0.45f), capsuleR);

        bool isActive = slider.isMouseOverOrDragging();
        g.setColour(isActive ? juce::Colour(palette.charcoalLight).withAlpha(0.95f)
                             : juce::Colour(palette.charcoalLight).withAlpha(0.55f));
        g.drawRoundedRectangle(capsule.reduced(0.5f), capsuleR, 0.8f);

        if (isVertical)
        {
            const float lineThick = 2.0f;
            float lx = capsule.getCentreX() - lineThick * 0.5f;
            float ly = capsule.getY() + 2.0f;
            float lh = capsule.getHeight() - 4.0f;
            if (lh > 1.0f)
            {
                g.setColour(accentColour.withAlpha(isActive ? 1.0f : 0.85f));
                g.fillRoundedRectangle(lx, ly, lineThick, lh, lineThick * 0.5f);
            }
        }
        else
        {
            const float lineThick = 2.0f;
            float lx = capsule.getCentreX() - lineThick * 0.5f;
            float ly = capsule.getY() + 4.0f;
            float lh = capsule.getHeight() - 8.0f;
            if (lh > 1.0f)
            {
                g.setColour(accentColour.withAlpha(isActive ? 1.0f : 0.9f));
                g.fillRoundedRectangle(lx, ly, lineThick, lh, lineThick * 0.5f);

                g.setColour(accentColour.withAlpha(isActive ? 0.4f : 0.25f));
                g.fillRoundedRectangle(lx - 1.0f, ly, lineThick + 2.0f, lh, (lineThick + 2.0f) * 0.5f);
            }
        }
    }
}

void CustomLookAndFeel::drawGraduationTicks(juce::Graphics &g, juce::Rectangle<float> trackRect, int numTicks,
                                            bool isVertical, bool small)
{
    const float majorLen = small ? 5.0f : 9.0f;
    const float minorLen = small ? 3.0f : 5.0f;
    const float majorThick = small ? 1.0f : 1.5f;
    const float minorThick = small ? 0.8f : 1.0f;
    const float majorAlpha = 0.65f;
    const float centreAlpha = 0.85f;
    const float minorAlpha = 0.32f;
    const float gap = small ? 2.0f : 3.0f;

    const int lastIdx = numTicks - 1;
    const int midIdx = lastIdx / 2;

    auto colour = juce::Colour(palette.warmWhite);

    for (int i = 0; i < numTicks; ++i)
    {
        const float t = (float)i / (float)lastIdx;

        const bool isEdge = (i == 0 || i == lastIdx);
        const bool isMid = (i == midIdx && (numTicks % 2) == 1);
        const bool isMajor = (isEdge || isMid);

        const float len = isMajor ? majorLen : minorLen;
        const float thick = isMajor ? majorThick : minorThick;
        float alpha = minorAlpha;
        if (isMid)
            alpha = centreAlpha;
        else if (isEdge)
            alpha = majorAlpha;

        g.setColour(colour.withAlpha(alpha));

        if (isVertical)
        {
            const float ty = trackRect.getY() + t * trackRect.getHeight();
            g.fillRect(juce::Rectangle<float>(trackRect.getX() - gap - len, ty - thick * 0.5f, len, thick));
            g.fillRect(juce::Rectangle<float>(trackRect.getRight() + gap, ty - thick * 0.5f, len, thick));
        }
        else
        {
            const float tx = trackRect.getX() + t * trackRect.getWidth();
            g.fillRect(juce::Rectangle<float>(tx - thick * 0.5f, trackRect.getY() - gap - len, thick, len));
            g.fillRect(juce::Rectangle<float>(tx - thick * 0.5f, trackRect.getBottom() + gap, thick, len));
        }
    }
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                                         float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider &slider)
{
    auto accentColour = slider.isColourSpecified(juce::Slider::rotarySliderFillColourId)
                            ? slider.findColour(juce::Slider::rotarySliderFillColourId)
                            : juce::Colour(palette.terracotta);

    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(8.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = radius * 0.2f;
    auto arcRadius = radius - lineW * 0.5f;

    juce::Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle,
                                rotaryEndAngle, true);

    g.setColour(juce::Colour(palette.charcoalLight));
    g.strokePath(backgroundArc,
                 juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle,
                           toAngle, true);

    g.setColour(accentColour);
    g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto &props = slider.getProperties();
    if ((bool)props[getModActivePropertyId()])
    {
        const float modAmount = juce::jlimit(-1.0f, 1.0f, (float)props[getModAmountPropertyId()]);
        const float modPos = juce::jlimit(0.0f, 1.0f, sliderPosProportional + modAmount);
        const float modAngle = rotaryStartAngle + modPos * (rotaryEndAngle - rotaryStartAngle);

        const float modRingRadius = arcRadius + lineW * 1.45f;
        const float modRingW = lineW * 0.45f;

        juce::Path modTrack;
        modTrack.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), modRingRadius, modRingRadius, 0.0f,
                               rotaryStartAngle, rotaryEndAngle, true);

        g.setColour(juce::Colour(palette.steelGrey).withAlpha(0.18f));
        g.strokePath(modTrack,
                     juce::PathStrokeType(modRingW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (std::abs(modAngle - toAngle) > 0.001f)
        {
            juce::Path modArc;
            modArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), modRingRadius, modRingRadius, 0.0f,
                                 juce::jmin(toAngle, modAngle), juce::jmax(toAngle, modAngle), true);

            g.setColour(juce::Colour(palette.steelGrey).withAlpha(0.75f));
            g.strokePath(modArc,
                         juce::PathStrokeType(modRingW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        juce::Path modDot;
        const float dotSize = modRingW * 1.6f;
        modDot.addEllipse(-dotSize * 0.5f, -modRingRadius - dotSize * 0.5f, dotSize, dotSize);
        modDot.applyTransform(
            juce::AffineTransform::rotation(modAngle).translated(bounds.getCentreX(), bounds.getCentreY()));

        g.setColour(juce::Colour(palette.steelGrey));
        g.fillPath(modDot);
    }

    juce::Path pointer;
    auto pointerLength = radius * 0.6f;
    auto pointerThickness = lineW * 1.5f;
    pointer.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
    pointer.applyTransform(
        juce::AffineTransform::rotation(toAngle).translated(bounds.getCentreX(), bounds.getCentreY()));

    g.setColour(accentColour);
    g.fillPath(pointer);
}

void CustomLookAndFeel::drawScrollbar(juce::Graphics &g, juce::ScrollBar &scrollbar, int x, int y, int width,
                                      int height, bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                      bool isMouseOver, bool isMouseDown)
{
    g.setColour(scrollbar.findColour(juce::ScrollBar::backgroundColourId));
    g.fillRect((float)x, (float)y, (float)width, (float)height);

    juce::Rectangle<float> thumbBounds;

    if (isScrollbarVertical)
        thumbBounds = juce::Rectangle<float>((float)x, (float)thumbStartPosition, (float)width, (float)thumbSize);
    else
        thumbBounds = juce::Rectangle<float>((float)thumbStartPosition, (float)y, (float)thumbSize, (float)height);

    auto thumbColour = scrollbar.findColour(juce::ScrollBar::thumbColourId);

    if (isMouseDown)
        thumbColour = thumbColour.brighter(0.2f);
    else if (isMouseOver)
        thumbColour = thumbColour.brighter(0.1f);

    g.setColour(thumbColour);
    g.fillRoundedRectangle(thumbBounds, appTheme.borders.corner);
}

void CustomLookAndFeel::drawPopupMenuBackground(juce::Graphics &g, int width, int height)
{
    g.setColour(juce::Colour(palette.charcoalDark));
    g.fillRect(0, 0, width, height);
    g.setColour(juce::Colour(palette.steelGrey).withAlpha(0.5f));
    g.drawRect(0, 0, width, height, 1);
}

void CustomLookAndFeel::drawPopupMenuItem(juce::Graphics &g, const juce::Rectangle<int> &area, bool isSeparator,
                                          bool isActive, bool isHighlighted, bool isTicked, bool /*hasSubMenu*/,
                                          const juce::String &text, const juce::String & /*shortcutKeyText*/,
                                          const juce::Drawable * /*icon*/, const juce::Colour *textColourToUse)
{
    if (isSeparator)
    {
        auto r = area.reduced(5, 0);
        g.setColour(juce::Colour(palette.charcoalLight).withAlpha(0.5f));
        g.drawLine((float)r.getX(), (float)r.getCentreY(), (float)r.getRight(), (float)r.getCentreY(), 0.5f);
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour(juce::Colour(palette.steelGrey).withAlpha(0.15f));
        g.fillRect(area);
    }

    auto textColour = (textColourToUse != nullptr) ? *textColourToUse : juce::Colour(palette.warmWhite);
    if (!isActive)
        textColour = textColour.withAlpha(0.4f);
    g.setFont(juce::Font(juce::FontOptions(theme::FontFamily::load().notoRegular).withHeight(appTheme.typography.regular)));

    auto r = area.reduced(10, 0);
    if (isTicked)
    {
        auto tickArea = r.removeFromLeft(15).toFloat();
        g.setColour(juce::Colour(palette.steelGrey));
        g.fillEllipse(tickArea.withSizeKeepingCentre(6, 6));
    }

    g.setColour(textColour);
    g.drawText(text, r, juce::Justification::centredLeft, true);
}

void CustomLookAndFeel::fillTextEditorBackground(juce::Graphics &g, int width, int height, juce::TextEditor &textEditor)
{
    auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f);
    const float corner = appTheme.borders.corner;

    if (!textEditor.isEnabled())
    {
        g.setColour(juce::Colour(palette.charcoal).withAlpha(0.5f));
        g.fillRoundedRectangle(bounds, corner);
        return;
    }

    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillRoundedRectangle(bounds.translated(0, 1.0f), corner);

    g.setColour(juce::Colour(palette.charcoal));
    g.fillRoundedRectangle(bounds, corner);
}

void CustomLookAndFeel::drawTextEditorOutline(juce::Graphics &g, int width, int height, juce::TextEditor &textEditor)
{
    if (!textEditor.isEnabled())
        return;

    auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f);
    const float corner = appTheme.borders.corner;

    if (textEditor.hasKeyboardFocus(true))
    {
        g.setColour(juce::Colour(palette.steelGrey).withAlpha(0.15f));
        g.drawRoundedRectangle(bounds.expanded(1.5f), corner + 1.5f, 1.5f);

        g.setColour(juce::Colour(palette.steelGrey));
        g.drawRoundedRectangle(bounds, corner, 1.5f);
    }
    else if (textEditor.isMouseOver(true))
    {
        g.setColour(juce::Colour(palette.steelGrey).withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, corner, 1.0f);
    }
    else
    {
        g.setColour(juce::Colour(palette.steelGrey).withAlpha(0.4f));
        g.drawRoundedRectangle(bounds, corner, 0.8f);
    }
}

void CustomLookAndFeel::drawProgressBar(juce::Graphics &g, juce::ProgressBar &bar, int width, int height,
                                        double progress, const juce::String &textToShow)
{
    const auto background = bar.findColour(juce::ProgressBar::backgroundColourId);
    const auto foreground = bar.findColour(juce::ProgressBar::foregroundColourId);

    auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f);
    const float corner = appTheme.borders.corner;

    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillRoundedRectangle(bounds.translated(0, 1.0f), corner);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, corner);

    juce::Rectangle<float> filled;

    if (progress >= 0.0)
        filled = bounds.withWidth(bounds.getWidth() * (float)juce::jlimit(0.0, 1.0, progress));
    else
    {
        const float phase = (float)(juce::Time::getMillisecondCounter() % 2000) / 2000.0f;
        const float t = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
        const float segW = bounds.getWidth() * 0.3f;
        filled = bounds.withWidth(segW).withX(bounds.getX() + t * (bounds.getWidth() - segW));
    }

    if (filled.getWidth() > 1.0f)
    {
        g.setColour(foreground);
        g.fillRoundedRectangle(filled, corner);

        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(filled.withHeight(filled.getHeight() * 0.45f), corner);
    }

    g.setColour(foreground.brighter(0.2f).withAlpha(0.4f));
    g.drawRoundedRectangle(bounds, corner, appTheme.borders.sm);

    if (textToShow.isNotEmpty() && progress >= 0.0)
    {
        g.setFont(juce::Font(juce::FontOptions(theme::FontFamily::load().notoBold).withHeight(appTheme.typography.regular)));

        const auto textArea = juce::Rectangle<int>(0, 0, width, height);
        const auto filledInt = filled.getSmallestIntegerContainer();

        {
            juce::Graphics::ScopedSaveState save(g);
            g.excludeClipRegion(filledInt);
            g.setColour(juce::Colour(palette.warmWhite).withAlpha(0.9f));
            g.drawText(textToShow, textArea, juce::Justification::centred);
        }

        {
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(filledInt);
            g.setColour(background.darker(0.2f));
            g.drawText(textToShow, textArea, juce::Justification::centred);
        }
    }
}

juce::CaretComponent *CustomLookAndFeel::createCaretComponent(juce::Component *keyFocusOwner)
{
    auto *caret = new juce::CaretComponent(keyFocusOwner);
    caret->setColour(juce::CaretComponent::caretColourId, juce::Colour(palette.steelGrey));
    return caret;
}

const juce::Identifier &CustomLookAndFeel::getModAmountPropertyId()
{
    static const juce::Identifier id("modAmount");
    return id;
}

const juce::Identifier &CustomLookAndFeel::getModActivePropertyId()
{
    static const juce::Identifier id("modActive");
    return id;
}