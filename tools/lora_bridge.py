#!/usr/bin/env python3
"""Virtual LoRa link between the two Wokwi simulations.

Wokwi has no LoRa part and runs one firmware per simulation, so the truck node
and the farm node are two separate simulations that cannot see each other. In
SIM builds both nodes put their radio traffic on UART0 as lines prefixed
"LORA>"; this script carries those lines from one simulation to the other,
which is the only piece of the demo that a real SX1276 would replace.

    python tools/lora_bridge.py

Both simulations stream to the console, tagged [TRUCK] and [FARM], and every
frame that crosses the link is shown as [LINK]. Needs WOKWI_CLI_TOKEN in the
environment (https://wokwi.com/dashboard/ci).
"""

import argparse
import os
import shutil
import subprocess
import sys
import threading
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PREFIX = "LORA>"


def find_cli() -> str:
    found = shutil.which("wokwi-cli")
    if found:
        return found
    fallback = Path.home() / ".wokwi" / "bin" / (
        "wokwi-cli.exe" if os.name == "nt" else "wokwi-cli"
    )
    if fallback.exists():
        return str(fallback)
    sys.exit("wokwi-cli not found. Install it: iwr https://wokwi.com/ci/install.ps1 -useb | iex")


def pump_truck(proc, farm, log, quiet):
    """Truck output -> console; radio frames -> farm's serial input."""
    for raw in proc.stdout:
        line = raw.rstrip("\r\n")
        log.write(line + "\n")
        log.flush()

        at = line.find(PREFIX)
        if at < 0:
            if not quiet:
                print(f"[TRUCK] {line}", flush=True)
            continue

        frame = line[at:]
        print(f"[LINK ] {frame[len(PREFIX):]}", flush=True)
        try:
            farm.stdin.write(frame + "\n")
            farm.stdin.flush()
        except (BrokenPipeError, ValueError):
            return


def pump_farm(proc, log):
    for raw in proc.stdout:
        line = raw.rstrip("\r\n")
        log.write(line + "\n")
        log.flush()
        print(f"[FARM ] {line}", flush=True)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--scenario", default="scenario-remote.yaml",
                    help="truck-side scenario, or 'none' to drive the buttons by hand")
    ap.add_argument("--timeout", type=int, default=45000,
                    help="simulation milliseconds before each side gives up. Wokwi's free tier "
                         "also caps a run at 5 minutes of wall clock, and these simulations run "
                         "slower than real time, so keep this tight.")
    ap.add_argument("--quiet-truck", action="store_true",
                    help="show only radio frames from the truck, not its whole console")
    args = ap.parse_args()

    if not os.environ.get("WOKWI_CLI_TOKEN"):
        return print("WOKWI_CLI_TOKEN is not set - get one at https://wokwi.com/dashboard/ci") or 1

    cli = find_cli()
    logs = ROOT / "logs"
    logs.mkdir(exist_ok=True)

    farm_cmd = [cli, "base_node", "--interactive",
                "--timeout", str(args.timeout), "--timeout-exit-code", "0"]
    truck_cmd = [cli, ".", "--timeout", str(args.timeout), "--timeout-exit-code", "0"]
    if args.scenario != "none":
        truck_cmd += ["--scenario", args.scenario]

    common = dict(cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                  text=True, bufsize=1, encoding="utf-8", errors="replace")

    print(f"[BRIDGE] farm : {' '.join(farm_cmd)}", flush=True)
    farm = subprocess.Popen(farm_cmd, stdin=subprocess.PIPE, **common)
    print(f"[BRIDGE] truck: {' '.join(truck_cmd)}", flush=True)
    truck = subprocess.Popen(truck_cmd, **common)

    with open(logs / "truck.log", "w", encoding="utf-8") as tlog, \
         open(logs / "farm.log", "w", encoding="utf-8") as flog:
        threads = [
            threading.Thread(target=pump_truck, args=(truck, farm, tlog, args.quiet_truck),
                             daemon=True),
            threading.Thread(target=pump_farm, args=(farm, flog), daemon=True),
        ]
        for t in threads:
            t.start()

        try:
            truck.wait()
        except KeyboardInterrupt:
            pass
        finally:
            # Give the farm node a moment to render the last frame it received.
            try:
                farm.wait(timeout=8)
            except subprocess.TimeoutExpired:
                farm.terminate()
            for t in threads:
                t.join(timeout=3)

    print(f"[BRIDGE] done. logs in {logs}", flush=True)
    return truck.returncode or 0


if __name__ == "__main__":
    sys.exit(main())
