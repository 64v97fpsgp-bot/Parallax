#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class ParallaxAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::Timer
{
public:
    ParallaxAudioProcessorEditor (ParallaxAudioProcessor&);
    ~ParallaxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    ParallaxAudioProcessor& audioProcessor;

    // ── Valores exibidos (suavizados na UI thread) ──
    float displayCorr  =  1.0f;
    float displayWidth =  0.0f;
    float displayBal   =  0.0f;
    float displayMid   = -70.0f;
    float displaySide  = -70.0f;

    // ── Cópia local dos raios polares ──
    float displayRays    [ParallaxAudioProcessor::numRays] = {};
    float displayPeakRays[ParallaxAudioProcessor::numRays] = {};

    // ── Freeze ──
    bool frozen = false;

    // ── Cores (idênticas ao Meridian) ──
    juce::Colour colBg       { 0xff1c1c1e };
    juce::Colour colBgDeep   { 0xff141416 };
    juce::Colour colBgCard   { 0xff232325 };
    juce::Colour colBgMeter  { 0xff111113 };
    juce::Colour colBorder   { 0xff2e2e32 };
    juce::Colour colBorderHi { 0xff3a3a3e };
    juce::Colour colGreen    { 0xff32d74b };
    juce::Colour colYellow   { 0xffffd60a };
    juce::Colour colRed      { 0xffff453a };
    juce::Colour colWhite    { 0xffffffff };
    juce::Colour colGrayHi   { 0xccebebf5 };
    juce::Colour colGrayMid  { 0x66ebebf5 };
    juce::Colour colGrayLo   { 0x29ebebf5 };
    juce::Colour colPurple   { 0xffac00ff };
    juce::Colour colCyan     { 0xff00c1cb };

    // ── Layout rects ──
    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> bodyBounds;
    juce::Rectangle<int> polarBounds;
    juce::Rectangle<int> panelBounds;
    juce::Rectangle<int> footerBounds;

    // ── Draw helpers ──
    void drawHeader  (juce::Graphics&);
    void drawPolar   (juce::Graphics&);
    void drawPanel   (juce::Graphics&);
    void drawFooter  (juce::Graphics&);

    void drawMeterCard (juce::Graphics& g, juce::Rectangle<int> bounds,
                        const juce::String& label, const juce::String& value,
                        const juce::String& unit, float fillRatio,
                        juce::Colour barColour, bool showBar,
                        bool centerBar = false);

    void mouseDown (const juce::MouseEvent&) override;

    // ── Recursos embarcados ──
    juce::Typeface::Ptr typefaceRegular;
    juce::Typeface::Ptr typefaceSemiBold;
    juce::Typeface::Ptr typefaceBold;
    juce::Typeface::Ptr typefaceLightItalic;
    juce::Image logoImage;

    juce::Font fontR  (float size) const { return juce::Font(juce::FontOptions().withTypeface(typefaceRegular).withHeight(size));     }
    juce::Font fontSB (float size) const { return juce::Font(juce::FontOptions().withTypeface(typefaceSemiBold).withHeight(size));    }
    juce::Font fontB  (float size) const { return juce::Font(juce::FontOptions().withTypeface(typefaceBold).withHeight(size));        }
    juce::Font fontLI (float size) const { return juce::Font(juce::FontOptions().withTypeface(typefaceLightItalic).withHeight(size)); }

    // ── Botão freeze ──
    juce::Rectangle<int> freezeBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParallaxAudioProcessorEditor)
};
