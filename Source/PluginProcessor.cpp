#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
ParallaxAudioProcessor::ParallaxAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
    for (int i = 0; i < numRays; ++i)
    {
        polarRays[i].store(0.0f);
        polarPeakRays[i].store(0.0f);
        raySmoothBuf[i] = 0.0f;
    }
}

ParallaxAudioProcessor::~ParallaxAudioProcessor() {}

//==============================================================================
const juce::String ParallaxAudioProcessor::getName() const { return JucePlugin_Name; }
bool ParallaxAudioProcessor::acceptsMidi() const  { return false; }
bool ParallaxAudioProcessor::producesMidi() const { return false; }
bool ParallaxAudioProcessor::isMidiEffect() const { return false; }
double ParallaxAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int  ParallaxAudioProcessor::getNumPrograms()                          { return 1; }
int  ParallaxAudioProcessor::getCurrentProgram()                       { return 0; }
void ParallaxAudioProcessor::setCurrentProgram (int)                   {}
const juce::String ParallaxAudioProcessor::getProgramName (int)        { return {}; }
void ParallaxAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void ParallaxAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    sampleRate_   = sampleRate;
    currentSampleRate.store(sampleRate);

    // Decay do peak hold: cai ~6dB por segundo a 30fps de UI
    // Aqui calculamos por bloco de 512 samples
    // ~(44100/512) blocos/seg ≈ 86 blocos/seg
    // queremos cair em ~2 segundos → decay = pow(0.0, 1/(86*2)) ≈ 0.9942
    peakDecay = (float)std::pow(0.001, 1.0 / (sampleRate / blockSize * 2.0));

    resetMeters();
}

void ParallaxAudioProcessor::releaseResources() {}

void ParallaxAudioProcessor::resetMeters()
{
    blockSampleCount = 0;
    blockSumL  = blockSumR  = 0.0;
    blockSumLR = blockSumL2 = blockSumR2 = 0.0;

    smoothCorr  =  1.0f;
    smoothWidth =  0.0f;
    smoothBal   =  0.0f;
    smoothMid   = -70.0f;
    smoothSide  = -70.0f;

    correlation.store(1.0f);
    stereoWidth.store(0.0f);
    balance.store(0.0f);
    midLevel.store(-70.0f);
    sideLevel.store(-70.0f);
    hasSignal.store(false);

    for (int i = 0; i < numRays; ++i)
    {
        polarRays[i].store(0.0f);
        polarPeakRays[i].store(0.0f);
        raySmoothBuf[i] = 0.0f;
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ParallaxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}
#endif

//==============================================================================
void ParallaxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = getTotalNumInputChannels();

    // Garante limpeza dos canais extras
    for (int ch = numChannels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);

    // Precisa de stereo para análise
    if (numChannels < 2)
    {
        hasSignal.store(false);
        return;
    }

    const float* dataL = buffer.getReadPointer(0);
    const float* dataR = buffer.getReadPointer(1);

    // ── Detecta sinal ──
    float maxAbs = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        maxAbs = juce::jmax(maxAbs, std::abs(dataL[i]), std::abs(dataR[i]));

    const bool active = maxAbs > 3.16e-5f; // -90 dBFS
    hasSignal.store(active);

    if (!active)
    {
        // Decai suavemente quando sem sinal
        smoothCorr  = smoothCorr  + (1.0f  - smoothCorr)  * 0.005f;
        smoothWidth = smoothWidth + (0.0f  - smoothWidth)  * 0.005f;
        smoothBal   = smoothBal   + (0.0f  - smoothBal)    * 0.005f;
        smoothMid   = smoothMid   + (-70.0f - smoothMid)   * 0.005f;
        smoothSide  = smoothSide  + (-70.0f - smoothSide)  * 0.005f;

        correlation.store(smoothCorr);
        stereoWidth.store(smoothWidth);
        balance.store(smoothBal);
        midLevel.store(smoothMid);
        sideLevel.store(smoothSide);

        for (int i = 0; i < numRays; ++i)
        {
            float cur = polarRays[i].load();
            polarRays[i].store(cur * 0.97f);
            float peak = polarPeakRays[i].load();
            polarPeakRays[i].store(peak * peakDecay);
        }
        return;
    }

    // ── Acumula samples no bloco interno ──
    for (int i = 0; i < numSamples; ++i)
    {
        float L = dataL[i];
        float R = dataR[i];

        blockSumL2 += (double)(L * L);
        blockSumR2 += (double)(R * R);
        blockSumLR += (double)(L * R);
        blockSumL  += (double)std::abs(L);
        blockSumR  += (double)std::abs(R);

        blockSampleCount++;

        if (blockSampleCount >= blockSize)
        {
            const double n = (double)blockSampleCount;

            // ── Correlation: sum(L*R) / sqrt(sum(L²) * sum(R²)) ──
            double denom = std::sqrt(blockSumL2 * blockSumR2);
            float rawCorr = (denom > 1e-12)
                ? (float)juce::jlimit(-1.0, 1.0, blockSumLR / denom)
                : 1.0f;

            // ── Mid / Side RMS ──
            // M = (L+R)/sqrt(2), S = (L-R)/sqrt(2)
            // RMS_M² = (sumL2 + sumR2 + 2*sumLR) / (2*n)
            // RMS_S² = (sumL2 + sumR2 - 2*sumLR) / (2*n)
            double rmsM2 = (blockSumL2 + blockSumR2 + 2.0 * blockSumLR) / (2.0 * n);
            double rmsS2 = (blockSumL2 + blockSumR2 - 2.0 * blockSumLR) / (2.0 * n);
            rmsM2 = juce::jmax(0.0, rmsM2);
            rmsS2 = juce::jmax(0.0, rmsS2);

            float rawMid  = (rmsM2 > 1e-12) ? (float)(10.0 * std::log10(rmsM2)) : -70.0f;
            float rawSide = (rmsS2 > 1e-12) ? (float)(10.0 * std::log10(rmsS2)) : -70.0f;

            // ── Stereo Width ──
            // Usa ratio Side/Mid em dB, remapeado para 0-100%
            // -20dB = 0% (quase mono), 0dB = 100% (Side = Mid, muito aberto)
            // Música masterizada tipicamente fica entre -6 e -12dB → 40-70%
            float rawWidth = 0.0f;
            if (rmsM2 > 1e-12 && rmsS2 > 1e-12)
            {
                double ratioDb = 10.0 * std::log10(rmsS2 / rmsM2); // negativo = Side menor
                // Remap: -20dB → 0%, 0dB → 100%
                rawWidth = (float)juce::jlimit(0.0, 1.0, (ratioDb + 20.0) / 20.0);
            }

            // ── L/R Balance em dB ──
            double rmsL = blockSumL2 / n;
            double rmsR = blockSumR2 / n;
            float rawBal = 0.0f;
            if (rmsL > 1e-12 && rmsR > 1e-12)
                rawBal = (float)(10.0 * std::log10(rmsR / rmsL)); // positivo = mais R
            else if (rmsL < 1e-12 && rmsR > 1e-12)
                rawBal =  12.0f;
            else if (rmsR < 1e-12 && rmsL > 1e-12)
                rawBal = -12.0f;

            rawBal = juce::jlimit(-12.0f, 12.0f, rawBal);

            // ── Publica atômicos — smoothing feito na UI thread (timerCallback) ──
            correlation.store(rawCorr);
            stereoWidth.store(rawWidth);
            balance.store(rawBal);
            midLevel.store(rawMid);
            sideLevel.store(rawSide);

            // ── Leque polar ──
            // Cada raio representa um ângulo de pan
            // ângulo = arctan(R/L) → mapeado em -90 a +90 graus
            // energia do raio = sqrt(L² + R²) normalizado
            for (int ray = 0; ray < numRays; ++ray)
            {
                // ângulo do raio em graus: -90 (full L) a +90 (full R)
                float rayAngle = -90.0f + (float)ray / (float)(numRays - 1) * 180.0f;
                // pan em [-1, +1] correspondente ao ângulo
                float rayPan = std::tan(rayAngle * juce::MathConstants<float>::pi / 180.0f);
                rayPan = juce::jlimit(-8.0f, 8.0f, rayPan);

                // energia acumulada (suavizada) do raio anterior
                float prev = polarRays[ray].load();
                polarRays[ray].store(prev * 0.0f); // será recalculado por sample
            }

            // Recalcula os raios sample a sample neste bloco
            // Zera primeiro
            float rayAccum[numRays] = {};

            // Reprocessa os samples do bloco para os raios
            // (usamos os mesmos dataL/dataR, mas precisamos de índices locais)
            // Como já saímos do loop de samples, usamos os sums para estimar
            // a distribuição. Para um analyzer real, fazemos sample-accurate:
            // → por isso o loop de raios fica DENTRO do loop de samples abaixo.

            blockSumL  = blockSumR  = 0.0;
            blockSumLR = blockSumL2 = blockSumR2 = 0.0;
            blockSampleCount = 0;
        }
    }

    // ── Raios polares — calculados sample a sample (separado do bloco RMS) ──
    // Acumula energia por ângulo de pan
    float rayBlock[numRays] = {};
    int   rayCount = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        float L = dataL[i];
        float R = dataR[i];
        float energy = std::sqrt(L * L + R * R);

        if (energy > 1e-5f)
        {
            // Usa valores absolutos para colapsar os 4 quadrantes no semicirculo superior
            // abs(L) e abs(R) mapeiam sinal negativo no mesmo angulo que positivo
            float aL = std::abs(L);
            float aR = std::abs(R);

            // atan2(aR, aL): angulo de 0 (full L) a 90 (full R), 45 = centro/mono
            // Remap para -90 a +90: panDeg = atan2(aR,aL)*2 - 90
            // - full L: atan2(0,1)*2 - 90 = 0 - 90 = -90 (esquerda)
            // - mono (aL=aR): atan2(1,1)*2 - 90 = 90 - 90 = 0 (centro)
            // - full R: atan2(1,0)*2 - 90 = 180 - 90 = +90 (direita)
            float panDeg = std::atan2(aR, aL) * (180.0f / juce::MathConstants<float>::pi) * 2.0f - 90.0f;
            panDeg = juce::jlimit(-90.0f, 90.0f, panDeg);

            int rayIdx = (int)((panDeg + 90.0f) / 180.0f * (numRays - 1) + 0.5f);
            rayIdx = juce::jlimit(0, numRays - 1, rayIdx);
            rayBlock[rayIdx] += energy;
        }
        rayCount++;
    }

    // Normaliza e suaviza
    if (rayCount > 0)
    {
        float maxEnergy = 0.0f;
        for (int r = 0; r < numRays; ++r)
            maxEnergy = juce::jmax(maxEnergy, rayBlock[r]);

        for (int r = 0; r < numRays; ++r)
        {
            float norm = (maxEnergy > 1e-6f) ? (rayBlock[r] / maxEnergy) : 0.0f;
            // Subida mais lenta, descida bem lenta — movimento suave
            float alpha = norm > raySmoothBuf[r] ? 0.25f : 0.04f;
            raySmoothBuf[r] = raySmoothBuf[r] + (norm - raySmoothBuf[r]) * alpha;

            polarRays[r].store(raySmoothBuf[r]);

            // Peak hold
            float peak = polarPeakRays[r].load();
            if (raySmoothBuf[r] > peak)
                polarPeakRays[r].store(raySmoothBuf[r]);
            else
                polarPeakRays[r].store(peak * peakDecay);
        }
    }
}

//==============================================================================
bool ParallaxAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ParallaxAudioProcessor::createEditor()
{
    return new ParallaxAudioProcessorEditor(*this);
}

void ParallaxAudioProcessor::getStateInformation (juce::MemoryBlock&) {}
void ParallaxAudioProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParallaxAudioProcessor();
}
