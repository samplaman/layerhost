import wave
import struct
import math

sample_rate = 44100
duration = 2.0  # seconds
frequency = 440.0  # Hz

wave_file = wave.open('/home/lali/Desktop/layerhost/test.wav', 'w')
wave_file.setparams((2, 2, sample_rate, 0, 'NONE', 'not compressed'))

for i in range(int(sample_rate * duration)):
    t = float(i) / sample_rate
    value = int(32767.0 * 0.5 * math.sin(2.0 * math.pi * frequency * t))
    data = struct.pack('<hh', value, value)
    wave_file.writeframesraw(data)

wave_file.close()
print("Generated test.wav successfully")
