#include "apu.h"
#include <cstring>
#include <algorithm>
#include <cmath>

// ============================================================================
// Divisor table for noise channel
// ============================================================================
static const int NOISE_DIVISORS[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

// ============================================================================
// Square Channel 1 Implementation (with Sweep)
// ============================================================================

void SquareChannel1::reset() {
    enabled = false;
    dacEnabled = false;
    frequency = 0;
    frequencyTimer = 0;
    waveformPosition = 0;
    lengthCounter = 0;
    lengthEnabled = false;
    volume = 0;
    envelopeTimer = 0;
    envelopePace = 0;
    envelopeIncrease = false;
    sweepTimer = 0;
    sweepPace = 0;
    sweepStep = 0;
    sweepDecrease = false;
    sweepEnabled = false;
    shadowFrequency = 0;
}

void SquareChannel1::trigger() {
    // Enable channel if DAC is enabled
    enabled = dacEnabled;
    
    // Reload length counter if it was zero
    if (lengthCounter == 0) {
        lengthCounter = 64;
    }
    
    // Reload frequency timer (period = (2048 - frequency) * 4)
    frequencyTimer = (2048 - frequency) * 4;
    
    // Reload envelope
    volume = (NR12 >> 4) & 0x0F;
    envelopePace = NR12 & 0x07;
    envelopeIncrease = (NR12 & 0x08) != 0;
    envelopeTimer = envelopePace;
    
    // Initialize sweep
    shadowFrequency = frequency;
    sweepPace = (NR10 >> 4) & 0x07;
    sweepStep = NR10 & 0x07;
    sweepDecrease = (NR10 & 0x08) != 0;
    sweepTimer = sweepPace > 0 ? sweepPace : 8;
    sweepEnabled = (sweepPace > 0) || (sweepStep > 0);
    
    // Perform overflow check if step > 0
    if (sweepStep > 0) {
        uint16_t newFreq = calculateSweepFrequency();
        if (newFreq > 2047) {
            enabled = false;
        }
    }
}

void SquareChannel1::tickFrequency() {
    if (--frequencyTimer <= 0) {
        // Reload timer
        frequencyTimer = (2048 - frequency) * 4;
        // Advance waveform position
        waveformPosition = (waveformPosition + 1) & 7;
    }
}

void SquareChannel1::tickLength() {
    if (lengthEnabled && lengthCounter > 0) {
        if (--lengthCounter == 0) {
            enabled = false;
        }
    }
}

void SquareChannel1::tickEnvelope() {
    if (envelopePace == 0) return;
    
    if (--envelopeTimer <= 0) {
        envelopeTimer = envelopePace;
        
        if (envelopeIncrease && volume < 15) {
            volume++;
        } else if (!envelopeIncrease && volume > 0) {
            volume--;
        }
    }
}

uint16_t SquareChannel1::calculateSweepFrequency() {
    uint16_t newFreq = shadowFrequency >> sweepStep;
    
    if (sweepDecrease) {
        newFreq = shadowFrequency - newFreq;
    } else {
        newFreq = shadowFrequency + newFreq;
    }
    
    return newFreq;
}

void SquareChannel1::tickSweep() {
    if (--sweepTimer <= 0) {
        sweepTimer = sweepPace > 0 ? sweepPace : 8;
        
        if (sweepEnabled && sweepPace > 0) {
            uint16_t newFreq = calculateSweepFrequency();
            
            if (newFreq <= 2047 && sweepStep > 0) {
                shadowFrequency = newFreq;
                frequency = newFreq;
                
                // Perform overflow check again
                if (calculateSweepFrequency() > 2047) {
                    enabled = false;
                }
            }
            
            if (newFreq > 2047) {
                enabled = false;
            }
        }
    }
}

float SquareChannel1::getOutput() const {
    if (!enabled || !dacEnabled) return 0.0f;
    
    // Get duty cycle pattern
    uint8_t duty = (NR11 >> 6) & 0x03;
    uint8_t pattern = DUTY_PATTERNS[duty];
    
    // Check if current position in duty cycle is high
    bool high = (pattern >> (7 - waveformPosition)) & 1;
    
    // Convert to float (-1.0 to 1.0) with volume
    float sample = high ? 1.0f : -1.0f;
    return sample * (volume / 15.0f);
}

// ============================================================================
// Square Channel 2 Implementation (without Sweep)
// ============================================================================

void SquareChannel2::reset() {
    enabled = false;
    dacEnabled = false;
    frequency = 0;
    frequencyTimer = 0;
    waveformPosition = 0;
    lengthCounter = 0;
    lengthEnabled = false;
    volume = 0;
    envelopeTimer = 0;
    envelopePace = 0;
    envelopeIncrease = false;
}

void SquareChannel2::trigger() {
    enabled = dacEnabled;
    
    if (lengthCounter == 0) {
        lengthCounter = 64;
    }
    
    frequencyTimer = (2048 - frequency) * 4;
    
    volume = (NR22 >> 4) & 0x0F;
    envelopePace = NR22 & 0x07;
    envelopeIncrease = (NR22 & 0x08) != 0;
    envelopeTimer = envelopePace;
}

void SquareChannel2::tickFrequency() {
    if (--frequencyTimer <= 0) {
        frequencyTimer = (2048 - frequency) * 4;
        waveformPosition = (waveformPosition + 1) & 7;
    }
}

void SquareChannel2::tickLength() {
    if (lengthEnabled && lengthCounter > 0) {
        if (--lengthCounter == 0) {
            enabled = false;
        }
    }
}

void SquareChannel2::tickEnvelope() {
    if (envelopePace == 0) return;
    
    if (--envelopeTimer <= 0) {
        envelopeTimer = envelopePace;
        
        if (envelopeIncrease && volume < 15) {
            volume++;
        } else if (!envelopeIncrease && volume > 0) {
            volume--;
        }
    }
}

float SquareChannel2::getOutput() const {
    if (!enabled || !dacEnabled) return 0.0f;
    
    uint8_t duty = (NR21 >> 6) & 0x03;
    uint8_t pattern = DUTY_PATTERNS[duty];
    bool high = (pattern >> (7 - waveformPosition)) & 1;
    
    float sample = high ? 1.0f : -1.0f;
    return sample * (volume / 15.0f);
}

// ============================================================================
// Wave Channel Implementation
// ============================================================================

void WaveChannel::reset() {
    enabled = false;
    dacEnabled = false;
    frequency = 0;
    frequencyTimer = 0;
    waveformPosition = 0;
    sampleBuffer = 0;
    lengthCounter = 0;
    lengthEnabled = false;
    volumeShift = 0;
    
    // Initialize wave RAM with default pattern
    for (int i = 0; i < 16; i++) {
        waveRAM[i] = (i & 1) ? 0x00 : 0xFF;
    }
}

void WaveChannel::trigger() {
    enabled = dacEnabled;
    
    if (lengthCounter == 0) {
        lengthCounter = 256;
    }
    
    // Wave channel frequency timer period = (2048 - frequency) * 2
    frequencyTimer = (2048 - frequency) * 2;
    waveformPosition = 0;
}

void WaveChannel::tickFrequency() {
    if (--frequencyTimer <= 0) {
        frequencyTimer = (2048 - frequency) * 2;
        
        // Advance position (0-31)
        waveformPosition = (waveformPosition + 1) & 31;
        
        // Read sample from wave RAM
        // Each byte contains 2 samples (high nibble first, then low nibble)
        uint8_t byteIndex = waveformPosition / 2;
        if (waveformPosition & 1) {
            sampleBuffer = waveRAM[byteIndex] & 0x0F;
        } else {
            sampleBuffer = (waveRAM[byteIndex] >> 4) & 0x0F;
        }
    }
}

void WaveChannel::tickLength() {
    if (lengthEnabled && lengthCounter > 0) {
        if (--lengthCounter == 0) {
            enabled = false;
        }
    }
}

float WaveChannel::getOutput() const {
    if (!enabled || !dacEnabled) return 0.0f;
    
    // Get volume shift from NR32 (bits 5-6)
    // 0 = mute, 1 = 100%, 2 = 50%, 3 = 25%
    int shift = volumeShift;
    if (shift == 0) return 0.0f;
    
    // Apply volume shift
    uint8_t sample = sampleBuffer >> (shift - 1);
    
    // Convert 4-bit sample (0-15) to float (-1.0 to 1.0)
    return ((sample / 7.5f) - 1.0f);
}

// ============================================================================
// Noise Channel Implementation (LFSR)
// ============================================================================

void NoiseChannel::reset() {
    enabled = false;
    dacEnabled = false;
    lfsr = 0x7FFF;
    widthMode = false;
    frequencyTimer = 0;
    divisor = 0;
    clockShift = 0;
    lengthCounter = 0;
    lengthEnabled = false;
    volume = 0;
    envelopeTimer = 0;
    envelopePace = 0;
    envelopeIncrease = false;
}

void NoiseChannel::trigger() {
    enabled = dacEnabled;
    
    if (lengthCounter == 0) {
        lengthCounter = 64;
    }
    
    // Reset LFSR to all 1s
    lfsr = 0x7FFF;
    
    // Reload envelope
    volume = (NR42 >> 4) & 0x0F;
    envelopePace = NR42 & 0x07;
    envelopeIncrease = (NR42 & 0x08) != 0;
    envelopeTimer = envelopePace;
    
    // Calculate timer period
    divisor = NOISE_DIVISORS[NR43 & 0x07];
    clockShift = (NR43 >> 4) & 0x0F;
    frequencyTimer = divisor << clockShift;
}

void NoiseChannel::tickFrequency() {
    if (--frequencyTimer <= 0) {
        frequencyTimer = divisor << clockShift;
        
        // LFSR clock tick
        // XOR bits 0 and 1
        uint8_t xorResult = (lfsr & 0x01) ^ ((lfsr >> 1) & 0x01);
        
        // Shift LFSR right by 1
        lfsr >>= 1;
        
        // Put XOR result in bit 14 (high bit of 15-bit LFSR)
        lfsr |= (xorResult << 14);
        
        // If width mode (7-bit), also put in bit 6
        if (widthMode) {
            lfsr &= ~(1 << 6);
            lfsr |= (xorResult << 6);
        }
    }
}

void NoiseChannel::tickLength() {
    if (lengthEnabled && lengthCounter > 0) {
        if (--lengthCounter == 0) {
            enabled = false;
        }
    }
}

void NoiseChannel::tickEnvelope() {
    if (envelopePace == 0) return;
    
    if (--envelopeTimer <= 0) {
        envelopeTimer = envelopePace;
        
        if (envelopeIncrease && volume < 15) {
            volume++;
        } else if (!envelopeIncrease && volume > 0) {
            volume--;
        }
    }
}

float NoiseChannel::getOutput() const {
    if (!enabled || !dacEnabled) return 0.0f;
    
    // LFSR bit 0 inverted determines output
    // When bit 0 is 0, output is high
    bool high = (lfsr & 0x01) == 0;
    
    float sample = high ? 1.0f : -1.0f;
    return sample * (volume / 15.0f);
}

// ============================================================================
// APU Main Class Implementation
// ============================================================================

APU::APU() {
    reset();
}

void APU::reset() {
    channel1.reset();
    channel2.reset();
    channel3.reset();
    channel4.reset();
    
    NR50 = 0x77;
    NR51 = 0xF3;
    NR52 = 0xF1;
    
    masterEnabled = true;
    
    frameSequencerTimer = 0;
    frameSequencerStep = 0;
    
    sampleTimer = 0;
    sampleAccumulator = 0.0f;
    sampleCount = 0;
    
    // Reset audio filters
    lastLeftSample = 0.0f;
    lastRightSample = 0.0f;
    highPassLeft = 0.0f;
    highPassRight = 0.0f;
    
    bufferWritePos = 0;
    bufferReadPos = 0;
    samplesAvailable = 0;
    outputSamplesReady = 0;
    
    std::memset(audioBuffer, 0, sizeof(audioBuffer));
    std::memset(outputBuffer, 0, sizeof(outputBuffer));
}

void APU::setSampleRate(int rate) {
    hostSampleRate = rate;
    cyclesPerSample = CPU_CLOCK / rate;
}

void APU::tick(int cpuCycles) {
    if (!masterEnabled) return;
    
    for (int i = 0; i < cpuCycles; i++) {
        // Tick Frame Sequencer
        frameSequencerTimer++;
        if (frameSequencerTimer >= CYCLES_PER_FRAME_SEQ) {
            frameSequencerTimer = 0;
            tickFrameSequencer();
        }
        
        // Tick all channel frequency timers
        channel1.tickFrequency();
        channel2.tickFrequency();
        channel3.tickFrequency();
        channel4.tickFrequency();
        
        // Downsampling: accumulate samples
        sampleAccumulator += mixChannels();
        sampleCount++;
        
        // Generate host sample when enough cycles have passed
        sampleTimer++;
        if (sampleTimer >= cyclesPerSample) {
            sampleTimer = 0;
            generateSample();
        }
    }
}

void APU::tickFrameSequencer() {
    // Frame Sequencer runs at 512 Hz, divided into 8 steps
    // Step 0: Length
    // Step 1: -
    // Step 2: Length, Sweep
    // Step 3: -
    // Step 4: Length
    // Step 5: -
    // Step 6: Length, Sweep
    // Step 7: Envelope
    
    switch (frameSequencerStep) {
        case 0:
            channel1.tickLength();
            channel2.tickLength();
            channel3.tickLength();
            channel4.tickLength();
            break;
        case 2:
            channel1.tickLength();
            channel2.tickLength();
            channel3.tickLength();
            channel4.tickLength();
            channel1.tickSweep();
            break;
        case 4:
            channel1.tickLength();
            channel2.tickLength();
            channel3.tickLength();
            channel4.tickLength();
            break;
        case 6:
            channel1.tickLength();
            channel2.tickLength();
            channel3.tickLength();
            channel4.tickLength();
            channel1.tickSweep();
            break;
        case 7:
            channel1.tickEnvelope();
            channel2.tickEnvelope();
            channel4.tickEnvelope();
            break;
    }
    
    frameSequencerStep = (frameSequencerStep + 1) & 7;
}

float APU::mixChannels() {
    // Get output from each channel
    float ch1 = channel1.getOutput();
    float ch2 = channel2.getOutput();
    float ch3 = channel3.getOutput();
    float ch4 = channel4.getOutput();
    
    // Apply panning (NR51)
    float left = 0.0f;
    float right = 0.0f;
    
    // Left channel panning (bits 4-7)
    if (NR51 & 0x10) left += ch1;
    if (NR51 & 0x20) left += ch2;
    if (NR51 & 0x40) left += ch3;
    if (NR51 & 0x80) left += ch4;
    
    // Right channel panning (bits 0-3)
    if (NR51 & 0x01) right += ch1;
    if (NR51 & 0x02) right += ch2;
    if (NR51 & 0x04) right += ch3;
    if (NR51 & 0x08) right += ch4;
    
    // Apply master volume (NR50) - scaled down
    int leftVol = ((NR50 >> 4) & 0x07) + 1;
    int rightVol = (NR50 & 0x07) + 1;
    
    left *= leftVol / 8.0f;
    right *= rightVol / 8.0f;
    
    // Normalize (4 channels max)
    left /= 4.0f;
    right /= 4.0f;
    
    // Apply LOW-PASS FILTER to smooth harsh square waves
    // This emulates the analog filtering of real Game Boy hardware
    left = lastLeftSample + LP_FACTOR * (left - lastLeftSample);
    right = lastRightSample + LP_FACTOR * (right - lastRightSample);
    lastLeftSample = left;
    lastRightSample = right;
    
    // Apply HIGH-PASS FILTER to remove DC offset (prevents speaker "thump")
    float filteredLeft = left - highPassLeft;
    highPassLeft = left - filteredLeft * HP_FACTOR;
    
    float filteredRight = right - highPassRight;
    highPassRight = right - filteredRight * HP_FACTOR;
    
    // Mix to mono
    float mixed = (filteredLeft + filteredRight) * 0.5f;
    
    // Apply master volume
    mixed *= masterVolume;
    
    return mixed;
}

void APU::generateSample() {
    if (sampleCount == 0) return;
    
    // Average the accumulated samples for better quality
    float sample = sampleAccumulator / sampleCount;
    
    // Soft clipping using tanh - sounds much better than hard clipping
    // This prevents harsh digital distortion when audio is too loud
    if (sample > 0.8f || sample < -0.8f) {
        sample = std::tanh(sample);
    }
    
    // Final clamp to valid range (should rarely trigger after soft clip)
    sample = std::max(-1.0f, std::min(1.0f, sample));
    
    pushSample(sample);
    
    // Reset accumulator
    sampleAccumulator = 0.0f;
    sampleCount = 0;
}

void APU::pushSample(float sample) {
    if (samplesAvailable >= AUDIO_BUFFER_SIZE) {
        // Buffer full, drop oldest sample to prevent overflow
        bufferReadPos = (bufferReadPos + 1) % AUDIO_BUFFER_SIZE;
        samplesAvailable--;
    }
    
    audioBuffer[bufferWritePos] = sample;
    bufferWritePos = (bufferWritePos + 1) % AUDIO_BUFFER_SIZE;
    samplesAvailable++;
}

// ============================================================================
// Buffer Interface for Web Audio API
// ============================================================================

float* APU::getBufferPointer() {
    // Return pointer to the LINEAR output buffer (not the ring buffer)
    // JS should call fillOutputBuffer() first, then read from this pointer
    return outputBuffer;
}

float* APU::getOutputBuffer() {
    return outputBuffer;
}

int APU::getSamplesAvailable() {
    return samplesAvailable;
}

int APU::fillOutputBuffer(int maxSamples) {
    // Copy samples from ring buffer to linear output buffer
    // This is what JS should call before reading from getBufferPointer()
    
    if (maxSamples > OUTPUT_BUFFER_SIZE) {
        maxSamples = OUTPUT_BUFFER_SIZE;
    }
    
    int toCopy = (samplesAvailable < maxSamples) ? samplesAvailable : maxSamples;
    
    for (int i = 0; i < toCopy; i++) {
        outputBuffer[i] = audioBuffer[bufferReadPos];
        bufferReadPos = (bufferReadPos + 1) % AUDIO_BUFFER_SIZE;
    }
    
    samplesAvailable -= toCopy;
    outputSamplesReady = toCopy;
    
    // Fill remaining with silence to avoid pops
    for (int i = toCopy; i < maxSamples; i++) {
        outputBuffer[i] = 0.0f;
    }
    
    return toCopy;
}

void APU::clearBuffer() {
    bufferWritePos = 0;
    bufferReadPos = 0;
    samplesAvailable = 0;
    outputSamplesReady = 0;
    std::memset(audioBuffer, 0, sizeof(audioBuffer));
    std::memset(outputBuffer, 0, sizeof(outputBuffer));
}

void APU::consumeSamples(int count) {
    // This is now handled by fillOutputBuffer, but we keep it for compatibility
    if (count > samplesAvailable) {
        count = samplesAvailable;
    }
    bufferReadPos = (bufferReadPos + count) % AUDIO_BUFFER_SIZE;
    samplesAvailable -= count;
}

// ============================================================================
// Memory I/O Implementation ($FF10-$FF3F)
// ============================================================================

uint8_t APU::readByte(uint16_t address) {
    // Audio registers are at $FF10-$FF3F
    
    switch (address) {
        // Channel 1 (NR10-NR14)
        case 0xFF10: return channel1.NR10 | 0x80;
        case 0xFF11: return channel1.NR11 | 0x3F;  // Only bits 6-7 readable
        case 0xFF12: return channel1.NR12;
        case 0xFF13: return 0xFF;  // Write-only
        case 0xFF14: return channel1.NR14 | 0xBF;  // Only bit 6 readable
        
        // Channel 2 (NR21-NR24)
        case 0xFF16: return channel2.NR21 | 0x3F;
        case 0xFF17: return channel2.NR22;
        case 0xFF18: return 0xFF;  // Write-only
        case 0xFF19: return channel2.NR24 | 0xBF;
        
        // Channel 3 (NR30-NR34)
        case 0xFF1A: return channel3.NR30 | 0x7F;
        case 0xFF1B: return 0xFF;  // Write-only
        case 0xFF1C: return channel3.NR32 | 0x9F;
        case 0xFF1D: return 0xFF;  // Write-only
        case 0xFF1E: return channel3.NR34 | 0xBF;
        
        // Channel 4 (NR41-NR44)
        case 0xFF20: return 0xFF;  // Write-only
        case 0xFF21: return channel4.NR42;
        case 0xFF22: return channel4.NR43;
        case 0xFF23: return channel4.NR44 | 0xBF;
        
        // Control registers (NR50-NR52)
        case 0xFF24: return NR50;
        case 0xFF25: return NR51;
        case 0xFF26: {
            // NR52: Master control and channel status
            uint8_t status = NR52 & 0x80;  // Master enable
            if (channel1.enabled) status |= 0x01;
            if (channel2.enabled) status |= 0x02;
            if (channel3.enabled) status |= 0x04;
            if (channel4.enabled) status |= 0x08;
            return status | 0x70;  // Bits 4-6 always read as 1
        }
        
        // Wave RAM ($FF30-$FF3F)
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            return readWaveRAM(address);
        
        default:
            return 0xFF;
    }
}

void APU::writeByte(uint16_t address, uint8_t value) {
    // If APU is disabled, only NR52 can be written
    if (!masterEnabled && address != 0xFF26 && address < 0xFF30) {
        return;
    }
    
    switch (address) {
        // Channel 1 (NR10-NR14)
        case 0xFF10:
            channel1.NR10 = value;
            channel1.sweepPace = (value >> 4) & 0x07;
            channel1.sweepDecrease = (value & 0x08) != 0;
            channel1.sweepStep = value & 0x07;
            break;
        case 0xFF11:
            channel1.NR11 = value;
            channel1.lengthCounter = 64 - (value & 0x3F);
            break;
        case 0xFF12:
            channel1.NR12 = value;
            channel1.dacEnabled = (value & 0xF8) != 0;
            if (!channel1.dacEnabled) {
                channel1.enabled = false;
            }
            break;
        case 0xFF13:
            channel1.NR13 = value;
            channel1.frequency = (channel1.frequency & 0x700) | value;
            break;
        case 0xFF14:
            channel1.NR14 = value;
            channel1.frequency = (channel1.frequency & 0x00FF) | ((value & 0x07) << 8);
            channel1.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) {  // Trigger
                channel1.trigger();
            }
            break;
        
        // Channel 2 (NR21-NR24)
        case 0xFF16:
            channel2.NR21 = value;
            channel2.lengthCounter = 64 - (value & 0x3F);
            break;
        case 0xFF17:
            channel2.NR22 = value;
            channel2.dacEnabled = (value & 0xF8) != 0;
            if (!channel2.dacEnabled) {
                channel2.enabled = false;
            }
            break;
        case 0xFF18:
            channel2.NR23 = value;
            channel2.frequency = (channel2.frequency & 0x700) | value;
            break;
        case 0xFF19:
            channel2.NR24 = value;
            channel2.frequency = (channel2.frequency & 0x00FF) | ((value & 0x07) << 8);
            channel2.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) {  // Trigger
                channel2.trigger();
            }
            break;
        
        // Channel 3 (NR30-NR34)
        case 0xFF1A:
            channel3.NR30 = value;
            channel3.dacEnabled = (value & 0x80) != 0;
            if (!channel3.dacEnabled) {
                channel3.enabled = false;
            }
            break;
        case 0xFF1B:
            channel3.NR31 = value;
            channel3.lengthCounter = 256 - value;
            break;
        case 0xFF1C:
            channel3.NR32 = value;
            channel3.volumeShift = (value >> 5) & 0x03;
            break;
        case 0xFF1D:
            channel3.NR33 = value;
            channel3.frequency = (channel3.frequency & 0x700) | value;
            break;
        case 0xFF1E:
            channel3.NR34 = value;
            channel3.frequency = (channel3.frequency & 0x00FF) | ((value & 0x07) << 8);
            channel3.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) {  // Trigger
                channel3.trigger();
            }
            break;
        
        // Channel 4 (NR41-NR44)
        case 0xFF20:
            channel4.NR41 = value;
            channel4.lengthCounter = 64 - (value & 0x3F);
            break;
        case 0xFF21:
            channel4.NR42 = value;
            channel4.dacEnabled = (value & 0xF8) != 0;
            if (!channel4.dacEnabled) {
                channel4.enabled = false;
            }
            break;
        case 0xFF22:
            channel4.NR43 = value;
            channel4.divisor = NOISE_DIVISORS[value & 0x07];
            channel4.clockShift = (value >> 4) & 0x0F;
            channel4.widthMode = (value & 0x08) != 0;
            break;
        case 0xFF23:
            channel4.NR44 = value;
            channel4.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) {  // Trigger
                channel4.trigger();
            }
            break;
        
        // Control registers (NR50-NR52)
        case 0xFF24:
            NR50 = value;
            break;
        case 0xFF25:
            NR51 = value;
            break;
        case 0xFF26:
            NR52 = value;
            masterEnabled = (value & 0x80) != 0;
            if (!masterEnabled) {
                // Reset all registers when APU is disabled
                channel1.reset();
                channel2.reset();
                channel3.reset();
                channel4.reset();
                NR50 = 0;
                NR51 = 0;
            }
            break;
        
        // Wave RAM ($FF30-$FF3F)
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            writeWaveRAM(address, value);
            break;
    }
}

uint8_t APU::readWaveRAM(uint16_t address) {
    uint16_t offset = address - 0xFF30;
    
    // If channel 3 is playing, return the currently playing byte
    if (channel3.enabled) {
        return channel3.waveRAM[channel3.waveformPosition / 2];
    }
    
    return channel3.waveRAM[offset];
}

void APU::writeWaveRAM(uint16_t address, uint8_t value) {
    uint16_t offset = address - 0xFF30;
    
    // Writing while channel 3 is playing writes to the current position
    if (channel3.enabled) {
        channel3.waveRAM[channel3.waveformPosition / 2] = value;
    } else {
        channel3.waveRAM[offset] = value;
    }
}
