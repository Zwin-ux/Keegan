import wave
import math
import struct
import os
import random

def generate_sine_wave(filename, frequency=440, duration=2.0, volume=0.5, sample_rate=48000):
    print(f"Generating {filename}...")
    num_samples = int(duration * sample_rate)
    
    # Ensure directory exists
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16-bit
        wav_file.setframerate(sample_rate)
        
        for i in range(num_samples):
            # Sine wave
            t = float(i) / sample_rate
            value = int(volume * 32767.0 * math.sin(2.0 * math.pi * frequency * t))
            wav_file.writeframes(struct.pack('<h', value))

def generate_noise(filename, duration=2.0, volume=0.3, sample_rate=48000):
    print(f"Generating {filename}...")
    num_samples = int(duration * sample_rate)
    
    # Ensure directory exists
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(sample_rate)
        
        for i in range(num_samples):
            # White noise
            value = int(volume * 32767.0 * (random.random() * 2.0 - 1.0))
            wav_file.writeframes(struct.pack('<h', value))

# Base path
base_path = "assets/stems"

# Focus Room
generate_sine_wave(f"{base_path}/focus/base_drone.wav", frequency=110, duration=4.0, volume=0.4)
generate_sine_wave(f"{base_path}/focus/rhythm_tick.wav", frequency=880, duration=0.5, volume=0.2)
generate_noise(f"{base_path}/focus/texture_paper.wav", duration=2.0, volume=0.1)

# Rain Cave
generate_noise(f"{base_path}/rain/drone_water.wav", duration=5.0, volume=0.3)
generate_noise(f"{base_path}/rain/drops_layer.wav", duration=1.0, volume=0.2)
generate_sine_wave(f"{base_path}/rain/metal_echo.wav", frequency=330, duration=3.0, volume=0.15)

# Arcade Night
generate_sine_wave(f"{base_path}/arcade/neon_bed.wav", frequency=60, duration=4.0, volume=0.5)
generate_sine_wave(f"{base_path}/arcade/coin_echo.wav", frequency=1200, duration=0.2, volume=0.3)
generate_sine_wave(f"{base_path}/arcade/synth_line.wav", frequency=440, duration=2.0, volume=0.2)

# Sleep Ship
generate_sine_wave(f"{base_path}/sleep/engine_thrum.wav", frequency=55, duration=8.0, volume=0.5)
generate_noise(f"{base_path}/sleep/ventilation.wav", duration=4.0, volume=0.1)
generate_noise(f"{base_path}/sleep/hull_creak.wav", duration=1.0, volume=0.05)

print("Placeholder assets generated.")
