#ifndef APU_H
#define APU_H

#include <cstdint>

// ============================================================================
// Game Boy APU (Audio Processing Unit) - DMG-01
// ============================================================================
// Generates audio for all 4 sound channels without external libraries.
// Outputs float samples (-1.0 to 1.0) into a ring buffer for Web Audio API.
// ============================================================================

// Constants
constexpr int CPU_CLOCK = 4194304;           // Game Boy CPU frequency (Hz)
constexpr int HOST_SAMPLE_RATE = 44100;      // Target sample rate for Web Audio
constexpr int AUDIO_BUFFER_SIZE = 8192;      // Ring buffer size (samples) - larger for stability
constexpr int OUTPUT_BUFFER_SIZE = 4096;     // Linear output buffer for JS reads
constexpr int FRAME_SEQUENCER_RATE = 512;    // Frame sequencer frequency (Hz)
constexpr int CYCLES_PER_SAMPLE = CPU_CLOCK / HOST_SAMPLE_RATE;  // ~95 cycles
constexpr int CYCLES_PER_FRAME_SEQ = CPU_CLOCK / FRAME_SEQUENCER_RATE; // 8192 cycles
constexpr int SAMPLES_PER_FRAME = 44100 / 60; // ~735 samples per frame at 60 FPS

// Wave duty cycle patterns (12.5%, 25%, 50%, 75%)
constexpr uint8_t DUTY_PATTERNS[4] = {
    0b00000001,  // 12.5% duty
    0b00000011,  // 25% duty  
    0b00001111,  // 50% duty
    0b11111100   // 75% duty
};

// ============================================================================
// Channel 1: Square Wave with Sweep and Envelope
// ============================================================================
struct SquareChannel1 {
    // Registers (NR10-NR14)
    uint8_t NR10 = 0x80;  // Sweep: pace, direction, step
    uint8_t NR11 = 0xBF;  // Length timer and duty cycle
    uint8_t NR12 = 0xF3;  // Volume envelope
    uint8_t NR13 = 0x00;  // Frequency low 8 bits
    uint8_t NR14 = 0xBF;  // Trigger, length enable, frequency high 3 bits

    // Internal state
    bool enabled = false;
    bool dacEnabled = false;
    
    // Frequency/Timer
    uint16_t frequency = 0;      // 11-bit frequency
    int frequencyTimer = 0;      // Countdown timer
    int waveformPosition = 0;    // Position in duty cycle (0-7)
    
    // Length counter (64 steps max for square channels)
    int lengthCounter = 0;
    bool lengthEnabled = false;
    
    // Volume envelope
    int volume = 0;              // Current volume (0-15)
    int envelopeTimer = 0;       // Envelope countdown
    int envelopePace = 0;        // Envelope update rate
    bool envelopeIncrease = false;
    
    // Sweep
    int sweepTimer = 0;
    int sweepPace = 0;
    int sweepStep = 0;
    bool sweepDecrease = false;
    bool sweepEnabled = false;
    uint16_t shadowFrequency = 0;
    
    void reset();
    void trigger();
    void tickFrequency();
    void tickLength();
    void tickEnvelope();
    void tickSweep();
    uint16_t calculateSweepFrequency();
    float getOutput() const;
};

// ============================================================================
// Channel 2: Square Wave with Envelope (no Sweep)
// ============================================================================
struct SquareChannel2 {
    // Registers (NR21-NR24)
    uint8_t NR21 = 0x3F;  // Length timer and duty cycle
    uint8_t NR22 = 0x00;  // Volume envelope
    uint8_t NR23 = 0x00;  // Frequency low 8 bits
    uint8_t NR24 = 0xBF;  // Trigger, length enable, frequency high 3 bits

    // Internal state
    bool enabled = false;
    bool dacEnabled = false;
    
    // Frequency/Timer
    uint16_t frequency = 0;
    int frequencyTimer = 0;
    int waveformPosition = 0;
    
    // Length counter
    int lengthCounter = 0;
    bool lengthEnabled = false;
    
    // Volume envelope
    int volume = 0;
    int envelopeTimer = 0;
    int envelopePace = 0;
    bool envelopeIncrease = false;
    
    void reset();
    void trigger();
    void tickFrequency();
    void tickLength();
    void tickEnvelope();
    float getOutput() const;
};

// ============================================================================
// Channel 3: Wave Channel (arbitrary waveform from Wave RAM)
// ============================================================================
struct WaveChannel {
    // Registers (NR30-NR34)
    uint8_t NR30 = 0x7F;  // DAC enable
    uint8_t NR31 = 0xFF;  // Length timer
    uint8_t NR32 = 0x9F;  // Output level
    uint8_t NR33 = 0x00;  // Frequency low 8 bits
    uint8_t NR34 = 0xBF;  // Trigger, length enable, frequency high 3 bits

    // Wave RAM: 32 4-bit samples (stored in 16 bytes)
    uint8_t waveRAM[16] = {0};

    // Internal state
    bool enabled = false;
    bool dacEnabled = false;
    
    // Frequency/Timer
    uint16_t frequency = 0;
    int frequencyTimer = 0;
    int waveformPosition = 0;  // 0-31 position in wave RAM
    uint8_t sampleBuffer = 0;  // Last read sample
    
    // Length counter (256 steps max for wave channel)
    int lengthCounter = 0;
    bool lengthEnabled = false;
    
    // Volume shift (0=mute, 1=100%, 2=50%, 3=25%)
    int volumeShift = 0;
    
    void reset();
    void trigger();
    void tickFrequency();
    void tickLength();
    float getOutput() const;
};

// ============================================================================
// Channel 4: Noise Channel (LFSR - Linear Feedback Shift Register)
// ============================================================================
struct NoiseChannel {
    // Registers (NR41-NR44)
    uint8_t NR41 = 0xFF;  // Length timer
    uint8_t NR42 = 0x00;  // Volume envelope
    uint8_t NR43 = 0x00;  // Clock shift, LFSR width, divisor
    uint8_t NR44 = 0xBF;  // Trigger, length enable

    // Internal state
    bool enabled = false;
    bool dacEnabled = false;
    
    // LFSR (Linear Feedback Shift Register)
    uint16_t lfsr = 0x7FFF;  // 15-bit LFSR
    bool widthMode = false;   // false = 15-bit, true = 7-bit
    
    // Frequency/Timer
    int frequencyTimer = 0;
    int divisor = 0;
    int clockShift = 0;
    
    // Length counter
    int lengthCounter = 0;
    bool lengthEnabled = false;
    
    // Volume envelope
    int volume = 0;
    int envelopeTimer = 0;
    int envelopePace = 0;
    bool envelopeIncrease = false;
    
    void reset();
    void trigger();
    void tickFrequency();
    void tickLength();
    void tickEnvelope();
    float getOutput() const;
};

// ============================================================================
// APU Main Class
// ============================================================================
class APU {
public:
    APU();
    ~APU() = default;

    // Main tick function - call with CPU cycles elapsed
    void tick(int cpuCycles);
    
    // Memory I/O ($FF10-$FF3F)
    uint8_t readByte(uint16_t address);
    void writeByte(uint16_t address, uint8_t value);
    
    // Buffer interface for Web Audio API (Emscripten)
    float* getBufferPointer();           // Get pointer to OUTPUT buffer (linear)
    float* getOutputBuffer();            // Alias for getBufferPointer
    int getSamplesAvailable();           // Samples ready to read
    int fillOutputBuffer(int maxSamples); // Copy from ring to output buffer, returns count
    void clearBuffer();
    void consumeSamples(int count);      // Mark samples as consumed
    
    // Control
    void reset();
    bool isEnabled() const { return masterEnabled; }
    void setSampleRate(int rate);
    int getHostSampleRate() const { return hostSampleRate; }

private:
    // Audio channels
    SquareChannel1 channel1;
    SquareChannel2 channel2;
    WaveChannel channel3;
    NoiseChannel channel4;
    
    // Master control registers
    uint8_t NR50 = 0x77;  // Master volume and VIN panning
    uint8_t NR51 = 0xF3;  // Channel panning
    uint8_t NR52 = 0xF1;  // Audio master control
    
    bool masterEnabled = true;
    
    // Frame Sequencer (512 Hz clock)
    int frameSequencerTimer = 0;
    int frameSequencerStep = 0;  // 0-7 steps
    
    // Downsampling for host sample rate
    int sampleTimer = 0;
    int cyclesPerSample = CYCLES_PER_SAMPLE;
    int hostSampleRate = HOST_SAMPLE_RATE;
    
    // Sample accumulator for better downsampling
    float sampleAccumulator = 0.0f;
    int sampleCount = 0;
    
    // Audio filters for smoother output
    float lastLeftSample = 0.0f;   // For low-pass filter
    float lastRightSample = 0.0f;
    float highPassLeft = 0.0f;     // For DC offset removal (high-pass)
    float highPassRight = 0.0f;
    float masterVolume = 0.25f;    // Master volume (0.0 - 1.0) - lower to avoid clipping
    static constexpr float LP_FACTOR = 0.65f;   // Low-pass filter strength (0-1, higher = more filtering)
    static constexpr float HP_FACTOR = 0.999f;  // High-pass filter (removes DC offset)
    
    // Ring buffer for audio output (internal)
    float audioBuffer[AUDIO_BUFFER_SIZE] = {0};
    int bufferWritePos = 0;
    int bufferReadPos = 0;
    int samplesAvailable = 0;
    
    // Linear output buffer for JS (external reads)
    float outputBuffer[OUTPUT_BUFFER_SIZE] = {0};
    int outputSamplesReady = 0;
    
    // Internal methods
    void tickFrameSequencer();
    void tickChannels(int cycles);
    void generateSample();
    float mixChannels();
    void pushSample(float sample);
    
    // Wave RAM access helpers
    uint8_t readWaveRAM(uint16_t address);
    void writeWaveRAM(uint16_t address, uint8_t value);
};

#endif // APU_H
