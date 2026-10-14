#!/usr/bin/env python3
"""Summarise a Wokwi logic-analyzer VCD: per channel, segments of steady PWM
(frequency, duty) and short bursts (buzzer beeps). Times are simulation seconds.
Usage: vcdsum.py file.vcd [name=D0 ...]"""
import sys, bisect

def load(path):
    ids, t, ch = {}, 0, {}
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if line.startswith('$var'):
                p = line.split(); ids[p[3]] = p[4]; ch[p[4]] = []
            elif line.startswith('#'):
                t = int(line[1:])
            elif line and line[0] in '01' and line[1:] in ids:
                ev = ch[ids[line[1:]]]
                v = int(line[0])
                if not ev or ev[-1][1] != v: ev.append((t, v))
    return ch, t

def windows(ev, end, win=0.2e9):
    """yield (t0, freq_hz, duty) for consecutive windows"""
    ts = [e[0] for e in ev]
    out = []
    t0 = 0
    while t0 < end:
        t1 = min(t0 + win, end)
        i = bisect.bisect_right(ts, t0) - 1
        lvl = ev[i][1] if i >= 0 else 0
        high, rises, cur = 0.0, 0, t0
        j = i + 1
        while j < len(ev) and ev[j][0] < t1:
            if lvl: high += ev[j][0] - cur
            cur = ev[j][0]; lvl = ev[j][1]
            if lvl: rises += 1
            j += 1
        if lvl: high += t1 - cur
        dur = t1 - t0
        out.append((t0 / 1e9, rises / (dur / 1e9), high / dur))
        t0 = t1
    return out

def segments(ws, ftol=0.15, dtol=0.03):
    segs = []
    for t, f, d in ws:
        fr = round(f, -1) if f > 100 else round(f)
        if segs and abs(segs[-1][3] - d) <= dtol and (abs(segs[-1][2] - fr) <= max(2, ftol * fr)):
            segs[-1][1] = t
        else:
            segs.append([t, t, fr, d])
    return segs

def main():
    path = sys.argv[1]
    names = dict(a.split('=') for a in sys.argv[2:])
    ch, end = load(path)
    print(f'# {path}  length {end/1e9:.2f} s')
    for c, ev in ch.items():
        label = names.get(c, c)
        if not ev:
            print(f'{label}: no activity'); continue
        ws = windows(ev, end)
        print(f'## {label} ({len(ev)} edges)')
        for s0, s1, f, d in segments(ws):
            if s1 - s0 < 0.19 and d < 0.02: continue
            print(f'  {s0:7.2f}-{s1 + 0.2:7.2f} s  f={f:>6} Hz  duty={d*100:5.1f}%')

if __name__ == '__main__':
    main()
