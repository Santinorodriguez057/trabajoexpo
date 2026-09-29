import wave
import struct
import math
import random
import os

os.makedirs('assets', exist_ok=True)

# ----- Generate tension.wav (Low drone + heartbeat) -----
with wave.open('assets/tension.wav', 'w') as wav_file:
    wav_file.setnchannels(2)
    wav_file.setsampwidth(2)
    framerate = 32000
    wav_file.setframerate(framerate)
    
    duration = 5.0
    frames = []
    
    for i in range(int(framerate * duration)):
        t = i / framerate
        drone = math.sin(2 * math.pi * 55 * t) * 0.3
        beat_t = t % (60.0 / 80.0)
        pulse = 0.0
        if beat_t < 0.1:
            pulse = math.sin(2 * math.pi * 40 * beat_t * 10) * (0.1 - beat_t) * 10.0
        elif 0.2 < beat_t < 0.3:
            pulse = math.sin(2 * math.pi * 40 * (beat_t - 0.2) * 10) * (0.1 - (beat_t - 0.2)) * 5.0
            
        sample = (drone + pulse) * 32767
        sample = max(-32768, min(32767, int(sample)))
        # Append twice for stereo (Left and Right)
        frames.append(struct.pack('<hh', sample, sample))
        
    wav_file.writeframes(b''.join(frames))

# ----- Generate disparo.wav (Gunshot using filtered white noise) -----
with wave.open('assets/disparo.wav', 'w') as wav_file:
    wav_file.setnchannels(2)
    wav_file.setsampwidth(2)
    wav_file.setframerate(framerate)
    
    duration = 1.0
    frames = []
    
    for i in range(int(framerate * duration)):
        t = i / framerate
        noise = random.uniform(-1, 1)
        decay = math.exp(-t * 15.0)
        punch = math.sin(2 * math.pi * (100 - t * 80) * t) * math.exp(-t * 30.0)
        
        sample = (noise * decay * 0.7 + punch * 0.3) * 32767
        sample = max(-32768, min(32767, int(sample)))
        # Append twice for stereo
        frames.append(struct.pack('<hh', sample, sample))
        
    wav_file.writeframes(b''.join(frames))

print("Archivos generados en carpeta assets/ exitosamente!")
