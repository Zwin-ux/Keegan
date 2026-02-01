import wave
import math
import struct
import os
import random

def generate_voice_tone(filename, duration=3.0, base_freq=200):
    print(f"Generating voice {filename}...")
    sample_rate = 48000
    num_samples = int(duration * sample_rate)
    
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(sample_rate)
        
        for i in range(num_samples):
            t = float(i) / sample_rate
            
            # Simple formant-like synthesis (FM)
            mod = math.sin(2.0 * math.pi * 5.0 * t) * 50.0 
            carrier = math.sin(2.0 * math.pi * (base_freq + mod) * t)
            
            # Amplitude envelope (fade in/out)
            env = 1.0
            if t < 0.1: env = t / 0.1
            if t > duration - 0.1: env = (duration - t) / 0.1
            
            # Add some 'breath' noise
            noise = (random.random() * 2.0 - 1.0) * 0.3
            
            value = int(0.7 * env * (carrier * 0.7 + noise) * 32767.0)
            wav_file.writeframes(struct.pack('<h', value))

# Base path
base_path = "assets/voice"

generate_voice_tone(f"{base_path}/focus/library_quiet.wav", duration=2.5, base_freq=180)
generate_voice_tone(f"{base_path}/rain/water_remembers.wav", duration=3.5, base_freq=160)
generate_voice_tone(f"{base_path}/arcade/data_streams.wav", duration=2.0, base_freq=220)
generate_voice_tone(f"{base_path}/sleep/drifting.wav", duration=4.0, base_freq=140)

print("Voice assets generated.")
