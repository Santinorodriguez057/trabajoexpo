import wave
import struct
import math
import random
import os

os.makedirs('games/sdl2/tiro_con_arco/assets', exist_ok=True)
framerate = 44100

def write_wav(filename, samples):
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(2)
        wav_file.setsampwidth(2)
        wav_file.setframerate(framerate)
        frames = []
        for s in samples:
            val = max(-32768, min(32767, int(s * 32767)))
            frames.append(struct.pack('<hh', val, val))
        wav_file.writeframes(b''.join(frames))

# 1. tensar.wav - Wood strain / string stretch creak
duration = 1.2
samples = []
for i in range(int(framerate * duration)):
    t = i / framerate
    env = min(1.0, t / 0.2) * max(0.0, 1.0 - t / duration)
    # Pitch bend upwards as tension increases
    freq = 120 + 200 * (t / duration)
    carrier = math.sin(2 * math.pi * freq * t)
    # Intermittent creaking clicks
    creak = (random.random() - 0.5) * 0.4 if random.random() < 0.08 else 0.0
    harm = math.sin(2 * math.pi * freq * 2.3 * t) * 0.3
    s = (carrier * 0.4 + harm * 0.2 + creak) * env * 0.6
    samples.append(s)
write_wav('games/sdl2/tiro_con_arco/assets/tensar.wav', samples)

# 2. disparo.wav - Crisp bow twang and whoosh
duration = 0.5
samples = []
for i in range(int(framerate * duration)):
    t = i / framerate
    decay = math.exp(-t * 18.0)
    # Bowstring oscillation ~220Hz decaying
    twang = math.sin(2 * math.pi * 220 * t) * decay * 0.7
    twang += math.sin(2 * math.pi * 440 * t) * math.exp(-t * 28.0) * 0.3
    # Whoosh noise
    noise = (random.random() - 0.5) * math.exp(-((t - 0.04)**2) / 0.005) * 0.5
    s = twang + noise
    samples.append(s)
write_wav('games/sdl2/tiro_con_arco/assets/disparo.wav', samples)

# 3. impacto.wav - Deep wooden target thud
duration = 0.4
samples = []
for i in range(int(framerate * duration)):
    t = i / framerate
    decay = math.exp(-t * 22.0)
    thud = math.sin(2 * math.pi * 85 * (1.0 - t * 0.8) * t) * decay * 0.8
    wood = (random.random() - 0.5) * math.exp(-t * 50.0) * 0.5
    s = thud + wood
    samples.append(s)
write_wav('games/sdl2/tiro_con_arco/assets/impacto.wav', samples)

# 4. diana.wav - Bullseye chime + hit
duration = 0.8
samples = []
for i in range(int(framerate * duration)):
    t = i / framerate
    decay = math.exp(-t * 6.0)
    thud = math.sin(2 * math.pi * 90 * t) * math.exp(-t * 25.0) * 0.6
    # Golden resonant bell chord: C6, E6, G6
    chime1 = math.sin(2 * math.pi * 1046.5 * t) * decay * 0.35
    chime2 = math.sin(2 * math.pi * 1318.5 * t) * decay * 0.25
    chime3 = math.sin(2 * math.pi * 1567.98 * t) * decay * 0.2
    s = thud + chime1 + chime2 + chime3
    samples.append(s)
write_wav('games/sdl2/tiro_con_arco/assets/diana.wav', samples)

# 5. victoria.wav - Fanfare
duration = 1.2
samples = []
notes = [523.25, 659.25, 783.99, 1046.5] # C5, E5, G5, C6
for i in range(int(framerate * duration)):
    t = i / framerate
    note_idx = min(len(notes) - 1, int(t / 0.25))
    freq = notes[note_idx]
    sub_t = t - (note_idx * 0.25)
    env = math.exp(-sub_t * 5.0) if note_idx < 3 else math.exp(-sub_t * 2.0)
    wave_val = math.sin(2 * math.pi * freq * t) + 0.3 * math.sin(2 * math.pi * freq * 2 * t)
    s = wave_val * env * 0.4
    samples.append(s)
write_wav('games/sdl2/tiro_con_arco/assets/victoria.wav', samples)

print("Audio assets generated successfully!")
