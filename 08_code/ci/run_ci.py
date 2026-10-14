#!/usr/bin/env python3
"""Run Wokwi CLI scenarios (one or several nodes in parallel) and timestamp the console output.
Usage: run_ci.py <run-name> <project:scenario:timeout_ms> [<project:scenario:timeout_ms> ...]
"""
import os, sys, subprocess, threading, time, re

CLI = os.environ.get("WOKWI_CLI", "wokwi-cli")
ROOT = os.path.dirname(os.path.abspath(__file__))

def main():
    run = sys.argv[1]
    jobs = [j.split(":") for j in sys.argv[2:]]
    env = dict(os.environ)
    if "WOKWI_CLI_TOKEN" not in env:          # token never lives in the repo
        tf = env.get("WOKWI_TOKEN_FILE")
        if not tf: sys.exit("set WOKWI_CLI_TOKEN or WOKWI_TOKEN_FILE")
        env["WOKWI_CLI_TOKEN"] = open(tf).read().strip()
    t0 = time.time()
    procs = []
    STAGGER = float(os.environ.get('STAGGER', '0'))
    for i, (proj, scen, tmo) in enumerate(jobs):
        if i and STAGGER: time.sleep(STAGGER)
        out = os.path.join(ROOT, proj, "out", run)
        os.makedirs(out, exist_ok=True)
        for d in re.findall(r"save-to: (\S+)/[^/\s]+\.png", open(os.path.join(ROOT, proj, scen)).read()):
            os.makedirs(os.path.join(ROOT, proj, d), exist_ok=True)
        cmd = [CLI, os.path.join(ROOT, proj), "--scenario", scen, "--timeout", tmo,
               "--serial-log-file", os.path.join(out, "serial.log"),
               "--vcd-file", os.path.join(out, "logic_analyzer.vcd")]
        p = subprocess.Popen(cmd, cwd=os.path.join(ROOT, proj), env=env,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        log = open(os.path.join(out, "console.log"), "w")
        def pump(p=p, log=log, name=proj):
            for line in p.stdout:
                log.write(f"[wall {time.time() - t0:8.3f}] {line}")
                log.flush()
            log.close()
        th = threading.Thread(target=pump, daemon=True); th.start()
        procs.append((proj, p, th))
    for proj, p, th in procs:
        rc = p.wait(); th.join()
        print(f"{run} {proj}: exit {rc}  ({time.time() - t0:.1f}s wall)")

if __name__ == "__main__":
    main()
