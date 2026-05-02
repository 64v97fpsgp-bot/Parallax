#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ParallaxAudioProcessorEditor::ParallaxAudioProcessorEditor (ParallaxAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (620, 480);

    typefaceRegular  = juce::Typeface::createSystemTypefaceFor(
        BinaryData::BarlowCondensedRegular_ttf,
        BinaryData::BarlowCondensedRegular_ttfSize);
    typefaceSemiBold = juce::Typeface::createSystemTypefaceFor(
        BinaryData::BarlowCondensedSemiBold_ttf,
        BinaryData::BarlowCondensedSemiBold_ttfSize);
    typefaceBold     = juce::Typeface::createSystemTypefaceFor(
        BinaryData::BarlowCondensedBold_ttf,
        BinaryData::BarlowCondensedBold_ttfSize);
    typefaceLightItalic = juce::Typeface::createSystemTypefaceFor(
        BinaryData::BarlowCondensedLightItalic_ttf,
        BinaryData::BarlowCondensedLightItalic_ttfSize);

    logoImage = juce::ImageCache::getFromMemory(
        BinaryData::SMLLOGOParallax_png,
        BinaryData::SMLLOGOParallax_pngSize);

    startTimerHz(30);
}

ParallaxAudioProcessorEditor::~ParallaxAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void ParallaxAudioProcessorEditor::timerCallback()
{
    if (frozen) return;

    const bool active = audioProcessor.hasSignal.load();

    float rawCorr  = audioProcessor.correlation.load();
    float rawWidth = audioProcessor.stereoWidth.load();
    float rawBal   = audioProcessor.balance.load();
    float rawMid   = audioProcessor.midLevel.load();
    float rawSide  = audioProcessor.sideLevel.load();

    // Lerp suave na UI thread (30fps)
    auto lerp = [](float cur, float tgt, float a) { return cur + (tgt - cur) * a; };

    if (active)
    {
        // Alpha 0.04 a 30fps = ~70% convergência em 1 segundo — lento e legível
        displayCorr  = lerp(displayCorr,  rawCorr,  0.04f);
        displayWidth = lerp(displayWidth, rawWidth,  0.04f);
        displayBal   = lerp(displayBal,   rawBal,    0.03f);
        displayMid   = lerp(displayMid,   rawMid,    0.04f);
        displaySide  = lerp(displaySide,  rawSide,   0.04f);
    }
    else
    {
        displayCorr  = lerp(displayCorr,   1.0f,  0.02f);
        displayWidth = lerp(displayWidth,  0.0f,  0.02f);
        displayBal   = lerp(displayBal,    0.0f,  0.02f);
        displayMid   = lerp(displayMid,  -70.0f,  0.02f);
        displaySide  = lerp(displaySide, -70.0f,  0.02f);
    }

    // Copia raios polares do processor
    for (int i = 0; i < ParallaxAudioProcessor::numRays; ++i)
    {
        displayRays[i]     = audioProcessor.polarRays[i].load();
        displayPeakRays[i] = audioProcessor.polarPeakRays[i].load();
    }

    repaint();
}

//==============================================================================
void ParallaxAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    headerBounds = b.removeFromTop(56);
    footerBounds = b.removeFromBottom(44);
    bodyBounds   = b;

    // Corpo: polar à esquerda, painel à direita
    polarBounds = bodyBounds.withTrimmedRight(186);
    panelBounds = bodyBounds.withTrimmedLeft(bodyBounds.getWidth() - 186);
}

//==============================================================================
void ParallaxAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(colBg);
    drawHeader(g);
    drawPolar(g);
    drawPanel(g);
    drawFooter(g);
}

//==============================================================================
// HEADER — idêntico ao Meridian, só troca o nome
void ParallaxAudioProcessorEditor::drawHeader (juce::Graphics& g)
{
    auto b = headerBounds;
    g.setColour(colBgDeep);
    g.fillRect(b);

    // Linha roxa+ciano no topo
    juce::ColourGradient topLine(colPurple, (float)b.getX(), 0,
                                  colCyan,  (float)b.getRight() * 0.6f, 0, false);
    topLine.addColour(1.0, juce::Colours::transparentBlack);
    g.setGradientFill(topLine);
    g.fillRect(b.getX(), b.getY(), b.getWidth(), 2);

    g.setColour(colBorder);
    g.drawLine(b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 1.0f);

    if (logoImage.isValid())
    {
        int logoH = b.getHeight() - 20;
        int logoW = (int)((float)logoImage.getWidth() / (float)logoImage.getHeight() * logoH);
        g.drawImage(logoImage, 14, b.getY() + 10, logoW, logoH,
                    0, 0, logoImage.getWidth(), logoImage.getHeight());
        g.setColour(colBorderHi);
        g.fillRect(14 + logoW + 16, b.getY() + 12, 1, b.getHeight() - 24);
        g.setColour(colGrayMid);
        g.setFont(fontR(24.0f));
        g.drawText("PARALLAX", b.withLeft(14 + logoW + 28), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour(colWhite);
        g.setFont(fontB(22.0f));
        g.drawText("SML", b.withWidth(54).withTrimmedLeft(14), juce::Justification::centredLeft);
        g.setColour(colBorderHi);
        g.fillRect(58, b.getY() + 12, 1, b.getHeight() - 24);
        g.setColour(colGrayMid);
        g.setFont(fontR(24.0f));
        g.drawText("PARALLAX", b.withLeft(66), juce::Justification::centredLeft);
    }

    g.setColour(colGrayLo);
    g.setFont(fontR(14.0f));
    g.drawText("v1.0", b.withTrimmedRight(14), juce::Justification::centredRight);

    g.setColour(colGrayLo);
    g.setFont(fontLI(11.0f));
    g.drawText("crafted by Henrique Ravage",
               b.withTrimmedRight(56).withTrimmedLeft(getWidth() / 2 - 60),
               juce::Justification::centredRight);
}

//==============================================================================
void ParallaxAudioProcessorEditor::drawPolar (juce::Graphics& g)
{
    auto b = polarBounds;
    g.setColour(colBgDeep.brighter(0.03f));
    g.fillRect(b);

    // Borda direita separando do painel
    g.setColour(colBorder);
    g.drawLine(b.getRight(), b.getY(), b.getRight(), b.getBottom(), 1.0f);

    const int numRays = ParallaxAudioProcessor::numRays;

    // Centro do semicírculo — na borda inferior do polarBounds
    const float cx = (float)b.getCentreX();
    const float cy = (float)b.getBottom() - 24.0f;

    // Raio máximo disponível
    const float maxR = juce::jmin((float)b.getWidth() * 0.88f,
                                   cy - (float)b.getY() - 16.0f);

    // ── Grid: anéis de referência ──
    const float ringR[] = { maxR * 0.90f, maxR * 0.70f, maxR * 0.45f, maxR * 0.20f };
    const float dbLevels[] = { -20.0f, -40.0f, -60.0f, -80.0f };

    for (int ri = 0; ri < 4; ++ri)
    {
        g.setColour(ri == 0 ? juce::Colour(0xff3a3a3e) : juce::Colour(0xff232325));
        g.drawEllipse(cx - ringR[ri], cy - ringR[ri],
                      ringR[ri] * 2.0f, ringR[ri] * 2.0f, ri == 0 ? 1.0f : 0.5f);
        g.setColour(juce::Colour(0xff3a3a3e));
        g.setFont(fontR(11.0f));
        g.drawText(juce::String((int)dbLevels[ri]),
                   (int)(cx - ringR[ri] - 28), (int)(cy - 7), 26, 14,
                   juce::Justification::centredRight);
    }

    // ── Spokes angulares ──
    // Angulos: -90=L, -45=limite saudavel esq, 0=C, +45=limite saudavel dir, +90=R
    // spokeAngles mapeados: nosso sistema usa -90(L) a +90(R), centro=0
    // Limite saudavel = ±45° no nosso sistema
    const float spokeAngles[] = { -90.0f, -45.0f, -22.5f, 0.0f, 22.5f, 45.0f, 90.0f };
    const char* spokeLabels[] = { "L", "", "", "C", "", "", "R" };
    // -45 e +45 são os limites saudáveis (equivale a panning completo L ou R)

    for (int si = 0; si < 7; ++si)
    {
        float angRad  = spokeAngles[si] * juce::MathConstants<float>::pi / 180.0f;
        float ex = cx + maxR * std::sin(angRad);
        float ey = cy - maxR * std::cos(angRad);

        bool isCenter  = (si == 3);
        bool isEdge    = (si == 0 || si == 6);
        bool isLimit   = (si == 1 || si == 5); // ±45° — limite saudável

        if (isLimit)
        {
            // Linha destacada em amarelo suave — limite de saude do stereo field
            g.setColour(colYellow.withAlpha(0.30f));
            g.drawLine(cx, cy, ex, ey, 1.2f);
        }
        else if (isCenter)
        {
            g.setColour(juce::Colour(0xff3a3a3e));
            g.drawLine(cx, cy, ex, ey, 1.0f);
        }
        else if (isEdge)
        {
            g.setColour(colRed.withAlpha(0.20f));
            juce::Path dashed;
            dashed.startNewSubPath(cx, cy);
            dashed.lineTo(ex, ey);
            float dash[] = { 4.0f, 4.0f };
            juce::PathStrokeType(0.5f).createDashedStroke(dashed, dashed, dash, 2);
            g.strokePath(dashed, juce::PathStrokeType(0.5f));
        }
        else
        {
            g.setColour(juce::Colour(0xff252528));
            juce::Path dashed;
            dashed.startNewSubPath(cx, cy);
            dashed.lineTo(ex, ey);
            float dash[] = { 3.0f, 5.0f };
            juce::PathStrokeType(0.5f).createDashedStroke(dashed, dashed, dash, 2);
            g.strokePath(dashed, juce::PathStrokeType(0.5f));
        }

        // Labels L, C, R
        if (juce::String(spokeLabels[si]).isNotEmpty())
        {
            float lx = cx + (maxR + 14.0f) * std::sin(angRad);
            float ly = cy - (maxR + 14.0f) * std::cos(angRad);
            g.setColour(isEdge ? colRed.withAlpha(0.6f) : colGrayMid);
            g.setFont(fontB(15.0f));
            g.drawText(spokeLabels[si], (int)lx - 12, (int)ly - 10, 24, 20,
                       juce::Justification::centred);
        }
    }

    // ── Anti-phase labels ──
    g.setColour(colRed.withAlpha(0.45f));
    g.setFont(fontR(11.0f));
    g.drawText("Anti-phase", b.getX() + 4, (int)cy - 16, 76, 14,
               juce::Justification::centredLeft);
    g.drawText("Anti-phase", b.getRight() - 80, (int)cy - 16, 76, 14,
               juce::Justification::centredRight);

    // ── Fill polar ──
    // Estratégia: fill sempre fecha pelo centro (cx,cy)
    // Stroke só conecta raios com energia > threshold para evitar linhas espurias
    // Em mono: raio central tem valor alto, laterais têm 0 → aparece só linha central
    const float rayThreshold = 0.003f;

    // Fill — passa por todos os raios, zeros colapsam em cx,cy
    juce::Path fillPath;
    fillPath.startNewSubPath(cx, cy);
    for (int ri = 0; ri < numRays; ++ri)
    {
        float angDeg = -90.0f + (float)ri / (float)(numRays - 1) * 180.0f;
        float angRad = angDeg * juce::MathConstants<float>::pi / 180.0f;
        float val    = displayRays[ri];
        float r      = (val > rayThreshold ? val : 0.0f) * maxR * 0.88f;
        fillPath.lineTo(cx + r * std::sin(angRad), cy - r * std::cos(angRad));
    }
    fillPath.lineTo(cx, cy);
    fillPath.closeSubPath();
    g.setColour(colPurple.withAlpha(0.22f));
    g.fillPath(fillPath);

    // Stroke do leque — cada raio com energia desenha linha do centro ao ponto
    // Raios vizinhos com energia são conectados entre si (arco suave)
    // Raio isolado (ex: mono) ainda aparece como linha do centro
    {
        juce::Path strokePath;
        bool inSegment = false;
        float prevPx = cx, prevPy = cy;

        for (int ri = 0; ri < numRays; ++ri)
        {
            float angDeg = -90.0f + (float)ri / (float)(numRays - 1) * 180.0f;
            float angRad = angDeg * juce::MathConstants<float>::pi / 180.0f;
            float val    = displayRays[ri];

            if (val > rayThreshold)
            {
                float r  = val * maxR * 0.88f;
                float px = cx + r * std::sin(angRad);
                float py = cy - r * std::cos(angRad);

                if (!inSegment)
                {
                    // Começa novo segmento — linha do centro até aqui
                    strokePath.startNewSubPath(cx, cy);
                    strokePath.lineTo(px, py);
                    inSegment = true;
                }
                else
                {
                    strokePath.lineTo(px, py);
                }
                prevPx = px; prevPy = py;
            }
            else
            {
                if (inSegment)
                {
                    // Fecha segmento de volta ao centro
                    strokePath.lineTo(cx, cy);
                    inSegment = false;
                }
            }
        }
        if (inSegment) strokePath.lineTo(cx, cy);

        g.setColour(colPurple.withAlpha(0.90f));
        g.strokePath(strokePath, juce::PathStrokeType(1.8f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Peak hold — mesmo padrão
    {
        juce::Path peakPath;
        bool inSegment = false;
        for (int ri = 0; ri < numRays; ++ri)
        {
            float angDeg = -90.0f + (float)ri / (float)(numRays - 1) * 180.0f;
            float angRad = angDeg * juce::MathConstants<float>::pi / 180.0f;
            float val    = displayPeakRays[ri];

            if (val > rayThreshold)
            {
                float r  = val * maxR * 0.88f;
                float px = cx + r * std::sin(angRad);
                float py = cy - r * std::cos(angRad);
                if (!inSegment) { peakPath.startNewSubPath(px, py); inSegment = true; }
                else            { peakPath.lineTo(px, py); }
            }
            else
            {
                inSegment = false;
            }
        }
        juce::Path dashedPeak;
        float dashArr[] = { 5.0f, 3.0f };
        juce::PathStrokeType(1.0f).createDashedStroke(dashedPeak, peakPath, dashArr, 2);
        g.setColour(colCyan.withAlpha(0.65f));
        g.strokePath(dashedPeak, juce::PathStrokeType(1.0f));
    }

    // ── Linha base horizontal ──
    g.setColour(juce::Colour(0xff3a3a3e));
    g.drawLine((float)b.getX() + 8.0f, cy, (float)b.getRight() - 8.0f, cy, 1.0f);

    // ── Freeze indicator ──
    if (frozen)
    {
        g.setColour(colYellow.withAlpha(0.15f));
        g.fillRect(b);
        g.setColour(colYellow.withAlpha(0.6f));
        g.setFont(fontB(13.0f));
        g.drawText("FROZEN", b.withHeight(16).withY(b.getY() + 6), juce::Justification::centred);
    }
}

//==============================================================================
void ParallaxAudioProcessorEditor::drawMeterCard (juce::Graphics& g,
                                                   juce::Rectangle<int> bounds,
                                                   const juce::String& label,
                                                   const juce::String& value,
                                                   const juce::String& unit,
                                                   float fillRatio,
                                                   juce::Colour barColour,
                                                   bool showBar,
                                                   bool centerBar)
{
    g.setColour(colBgMeter);
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
    g.setColour(colBorder);
    g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);

    // Label
    g.setColour(colGrayLo);
    g.setFont(fontSB(13.0f));
    g.drawText(label, bounds.withHeight(20).withTrimmedTop(6), juce::Justification::centred);

    // Value
    g.setColour(colWhite);
    g.setFont(fontB(26.0f));
    auto valueArea = bounds.withTrimmedTop(22).withTrimmedBottom(showBar ? 22 : 8);
    g.drawText(value, valueArea, juce::Justification::centred);

    // Unit (pequeno, ao lado)
    if (unit.isNotEmpty())
    {
        g.setColour(colGrayLo);
        g.setFont(fontR(12.0f));
        g.drawText(unit, bounds.withTrimmedTop(22).withTrimmedBottom(showBar ? 22 : 8)
                                .withTrimmedLeft(bounds.getWidth() / 2 + 20),
                   juce::Justification::centredLeft);
    }

    // Bar
    if (showBar)
    {
        auto barArea = bounds.withTrimmedTop(bounds.getHeight() - 18)
                             .withTrimmedBottom(6).withTrimmedLeft(8).withTrimmedRight(8);
        g.setColour(colBg);
        g.fillRoundedRectangle(barArea.toFloat(), 2.0f);

        if (centerBar)
        {
            // Bar centralizada (para balance)
            float half = (float)barArea.getWidth() / 2.0f;
            float fill = juce::jlimit(0.0f, half, std::abs(fillRatio) * half);
            float cx2  = (float)barArea.getX() + half;
            auto fillRect = fillRatio < 0
                ? juce::Rectangle<float>(cx2 - fill, (float)barArea.getY(), fill, (float)barArea.getHeight())
                : juce::Rectangle<float>(cx2,          (float)barArea.getY(), fill, (float)barArea.getHeight());
            g.setColour(barColour);
            g.fillRoundedRectangle(fillRect, 2.0f);

            // Centro tick
            g.setColour(colBorderHi);
            g.fillRect(cx2 - 0.5f, (float)barArea.getY(), 1.0f, (float)barArea.getHeight());
        }
        else
        {
            float fill = juce::jlimit(0.0f, 1.0f, fillRatio) * (float)barArea.getWidth();
            g.setColour(barColour);
            g.fillRoundedRectangle(juce::Rectangle<float>((float)barArea.getX(), (float)barArea.getY(),
                                                           fill, (float)barArea.getHeight()), 2.0f);
        }
    }
}

//==============================================================================
void ParallaxAudioProcessorEditor::drawPanel (juce::Graphics& g)
{
    auto b = panelBounds;
    g.setColour(colBgDeep);
    g.fillRect(b);

    const int pad    = 10;
    const int cardH  = 72;
    const int cardW  = b.getWidth() - pad * 2;
    const int startY = b.getY() + pad;

    // ── Correlation ──
    {
        auto card = juce::Rectangle<int>(b.getX() + pad, startY, cardW, cardH);

        // Cor da correlation
        float c = displayCorr;
        juce::Colour corrCol = c > 0.3f  ? colGreen :
                               c > -0.1f ? colYellow : colRed;

        juce::String corrStr = (c >= 0 ? "+" : "") + juce::String(c, 2);
        float barFill = (c + 1.0f) / 2.0f; // 0 a 1

        drawMeterCard(g, card, "CORRELATION", corrStr, "", barFill, corrCol, true, false);

        // Sub-labels -1 / 0 / +1
        g.setColour(colGrayLo);
        g.setFont(fontR(9.0f));
        auto barArea = card.withTrimmedTop(card.getHeight() - 18).withTrimmedBottom(6)
                           .withTrimmedLeft(8).withTrimmedRight(8);
        g.drawText("-1", barArea.withWidth(12), juce::Justification::centredLeft);
        g.drawText("0",  barArea,               juce::Justification::centred);
        g.drawText("+1", barArea.withTrimmedLeft(barArea.getWidth() - 12), juce::Justification::centredRight);
    }

    // ── Stereo Width ──
    {
        auto card = juce::Rectangle<int>(b.getX() + pad, startY + cardH + pad, cardW, cardH);
        juce::String widthStr = juce::String((int)std::round(displayWidth * 100)) + "%";
        drawMeterCard(g, card, "STEREO WIDTH", widthStr, "", displayWidth, colPurple, true, false);

        g.setColour(colGrayLo);
        g.setFont(fontR(9.0f));
        auto barArea = card.withTrimmedTop(card.getHeight() - 18).withTrimmedBottom(6)
                           .withTrimmedLeft(8).withTrimmedRight(8);
        g.drawText("MONO", barArea.withWidth(30), juce::Justification::centredLeft);
        g.drawText("WIDE", barArea.withTrimmedLeft(barArea.getWidth() - 30), juce::Justification::centredRight);
    }

    // ── L/R Balance ──
    {
        auto card = juce::Rectangle<int>(b.getX() + pad, startY + (cardH + pad) * 2, cardW, cardH);

        float balNorm = juce::jlimit(-1.0f, 1.0f, displayBal / 12.0f);
        juce::String balStr;
        if (std::abs(displayBal) < 0.3f)
            balStr = "C";
        else if (displayBal < 0)
            balStr = "L " + juce::String(std::abs(displayBal), 1);
        else
            balStr = "R " + juce::String(displayBal, 1);

        drawMeterCard(g, card, "L / R BALANCE", balStr, "dB", balNorm, colCyan, true, true);
    }

    // ── M/S Ratio (card completo, substitui Mid+Side) ──
    {
        auto card = juce::Rectangle<int>(b.getX() + pad, startY + (cardH + pad) * 3, cardW, cardH);

        float ratio = displayMid - displaySide;
        bool valid  = displayMid > -60.0f && displaySide > -60.0f;
        juce::String ratioStr = valid ? juce::String(ratio, 1) : "- -";

        juce::Colour ratioCol = !valid    ? colGrayMid :
                                ratio > 6  ? colGreen :
                                ratio > 0  ? colPurple : colCyan;

        float ratioFill = juce::jlimit(0.0f, 1.0f, (ratio + 12.0f) / 24.0f);

        drawMeterCard(g, card, "M / S RATIO", ratioStr, "dB", ratioFill, ratioCol, true, false);

        g.setColour(colGrayLo);
        g.setFont(fontR(9.0f));
        auto barArea = card.withTrimmedTop(card.getHeight() - 18).withTrimmedBottom(6)
                           .withTrimmedLeft(8).withTrimmedRight(8);
        g.drawText("WIDE", barArea.withWidth(28), juce::Justification::centredLeft);
        g.drawText("MONO", barArea.withTrimmedLeft(barArea.getWidth() - 28), juce::Justification::centredRight);
    }
}

//==============================================================================
// FOOTER — idêntico ao Meridian, adaptado sem presets
void ParallaxAudioProcessorEditor::drawFooter (juce::Graphics& g)
{
    auto b = footerBounds;
    g.setColour(colBgDeep);
    g.fillRect(b);

    g.setColour(colBorder);
    g.drawLine(b.getX(), b.getY(), b.getRight(), b.getY(), 1.0f);

    // ── Sample Rate badge (esquerda) ──
    double sr = audioProcessor.currentSampleRate.load();
    juce::String srStr;
    if      (sr >= 88200.0) srStr = juce::String((int)(sr / 1000)) + " kHz";
    else if (sr >= 44100.0) srStr = juce::String(sr / 1000.0, 1) + " kHz";
    else                    srStr = juce::String((int)sr) + " Hz";
    juce::String srFull = srStr + "  /  24 bit";

    auto srBadge = juce::Rectangle<int>(14, b.getY() + (b.getHeight() - 26) / 2, 130, 26);
    g.setColour(colCyan.withAlpha(0.1f));
    g.fillRoundedRectangle(srBadge.toFloat(), 4.0f);
    g.setColour(colCyan.withAlpha(0.5f));
    g.drawRoundedRectangle(srBadge.toFloat(), 4.0f, 1.0f);
    g.setColour(colCyan);
    g.setFont(fontSB(14.0f));
    g.drawText(srFull, srBadge, juce::Justification::centred);

    // ── Signal indicator (centro) ──
    bool active = audioProcessor.hasSignal.load();
    int cx2 = getWidth() / 2;
    float dotX = (float)cx2 - 50.0f;
    float dotY = (float)b.getCentreY() - 3.5f;
    g.setColour(active ? colGreen : colBorderHi);
    g.fillEllipse(dotX, dotY, 7.0f, 7.0f);
    g.setColour(active ? colGrayMid : colGrayLo);
    g.setFont(fontSB(13.0f));
    g.drawText(active ? "SIGNAL ACTIVE" : "NO SIGNAL",
               (int)dotX + 12, b.getY(), 120, b.getHeight(),
               juce::Justification::centredLeft);

    // ── Freeze button (direita) ──
    freezeBtn = juce::Rectangle<int>(getWidth() - 80, b.getY() + (b.getHeight() - 28) / 2, 64, 28);
    g.setColour(frozen ? colYellow.withAlpha(0.15f) : colBgCard);
    g.fillRoundedRectangle(freezeBtn.toFloat(), 4.0f);
    g.setColour(frozen ? colYellow : colBorderHi);
    g.drawRoundedRectangle(freezeBtn.toFloat(), 4.0f, 1.0f);
    g.setColour(frozen ? colYellow : colGrayMid);
    g.setFont(fontB(13.0f));
    g.drawText("FREEZE", freezeBtn, juce::Justification::centred);
}

//==============================================================================
void ParallaxAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (freezeBtn.contains(e.getPosition()))
    {
        frozen = !frozen;
        repaint();
    }
}
