# 本地音频检查工具：分析录音数据与素材信息，不承担游戏内音效播放。
"""Check the real mixer WAVs from -CharacterAudioRender, not request telemetry."""
import array
import json
import math
import pathlib
import sys
import wave

root = pathlib.Path(sys.argv[1])
report = []
for index in range(15):
    path = root / f'case_{index:02d}.wav'
    with wave.open(str(path), 'rb') as stream:
        assert stream.getsampwidth() == 2
        samples = array.array('h', stream.readframes(stream.getnframes()))
        duration = stream.getnframes() / stream.getframerate()
    assert samples and duration > 1.0, f'{path}: missing rendered frames'
    peak = max(abs(value) for value in samples) / 32768
    rms = math.sqrt(sum(value * value for value in samples) / len(samples)) / 32768
    clipped = sum(abs(value) >= 32767 for value in samples)
    if index in (4, 8, 12):
        assert peak < .001, f'{path}: silent action unexpectedly makes sound: {peak}'
    else:
        assert peak > .003 and rms > .0001, f'{path}: no meaningful audible output'
    assert clipped == 0, f'{path}: clipped PCM output'
    report.append({'case': index, 'seconds': round(duration, 3), 'peak': round(peak, 5),
                   'rms': round(rms, 5), 'clipped_samples': clipped})
print(json.dumps(report, indent=2))
print('PASS: all 15 real mixer recordings meet sound/silence and clipping checks')
