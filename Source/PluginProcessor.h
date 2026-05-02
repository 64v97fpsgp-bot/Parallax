#pragma once

#include <JuceHeader.h>
#include <atomic>

class ParallaxAudioProcessor : public juce::AudioProcessor
{
public:
    ParallaxAudioProcessor();
    ~ParallaxAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void resetMeters();

    // ── Valores lidos pela UI (thread-safe) ──
    std::atomic<float> correlation   {  1.0f };   // -1.0 a +1.0
    std::atomic<float> stereoWidth   {  0.0f };   // 0.0 a 1.0
    std::atomic<float> balance       {  0.0f };   // negativo = esquerda, positivo = direita (dB)
    std::atomic<float> midLevel      { -70.0f };  // dBFS
    std::atomic<float> sideLevel     { -70.0f };  // dBFS
    std::atomic<bool>  hasSignal     { false  };
    std::atomic<double> currentSampleRate { 44100.0 };

    // ── Dados do leque polar (ring buffer thread-safe via flag) ──
    // 65 raios cobrindo -90 a +90 graus
    static constexpr int numRays = 65;
    std::atomic<float> polarRays[numRays];  // energia por ângulo, 0.0 a 1.0
    std::atomic<float> polarPeakRays[numRays]; // peak hold

private:
    double sampleRate_ = 44100.0;

    // ── Janela RMS — acumula por bloco interno ──
    static constexpr int blockSize = 512;
    int    blockSampleCount = 0;

    double blockSumL  = 0.0;
    double blockSumR  = 0.0;
    double blockSumLR = 0.0;  // para correlação: sum(L*R)
    double blockSumL2 = 0.0;  // sum(L²)
    double blockSumR2 = 0.0;  // sum(R²)

    // ── Smoothing dos atômicos (no audio thread) ──
    float smoothCorr  =  1.0f;
    float smoothWidth =  0.0f;
    float smoothBal   =  0.0f;
    float smoothMid   = -70.0f;
    float smoothSide  = -70.0f;

    // ── Peak hold do leque polar ──
    float peakDecay = 0.9985f;  // decay por bloco

    // ── Smoothing dos raios polares (membro — reseta com resetMeters) ──
    float raySmoothBuf[numRays] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParallaxAudioProcessor)
};
