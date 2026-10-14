import sys
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from vcdsum import load
ch, end = load(sys.argv[1]); ev = ch[sys.argv[2]]
bursts = []; cur = None
for t, v in ev:
    if cur and t - cur[1] < 5e6: cur[1] = t; cur[2] += v
    else:
        if cur: bursts.append(cur)
        cur = [t, t, v]
if cur: bursts.append(cur)
for b0, b1, r in bursts:
    d = (b1 - b0) / 1e9
    if r > 2: print(f"beep at {b0/1e9:7.3f} s  length {d*1000:6.1f} ms  f={r/d if d else 0:7.1f} Hz")
