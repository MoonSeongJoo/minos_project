
import csv
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt


if len(sys.argv) != 2:
    print(
        "Usage: python3 plot_fallback.py <csv>"
    )
    sys.exit(1)

csv_path = Path(sys.argv[1])

with csv_path.open() as f:
    rows = list(csv.DictReader(f))

time_s = [
    float(r["time_s"])
    for r in rows
]

speed = [
    float(r["speed_kmh"])
    for r in rows
]

accel = [
    float(r["accel_mps2"])
    for r in rows
]

distance = [
    float(r["distance_m"])
    for r in rows
]

source = [
    1 if r["source"] == "FALLBACK" else 0
    for r in rows
]

trigger = next(
    float(r["time_s"])
    for r in rows
    if r["source"] == "FALLBACK"
)

stop_time = next(
    float(r["time_s"])
    for r in rows
    if int(r["hold_stop"]) == 1
)

fig, ax = plt.subplots(
    4,
    1,
    figsize=(12, 10),
    sharex=True
)

# Vehicle speed
ax[0].plot(
    time_s,
    speed,
    linewidth=2
)

ax[0].set_ylabel(
    "Speed (km/h)"
)

ax[0].set_title(
    "Vehicle Speed"
)

# Acceleration
ax[1].plot(
    time_s,
    accel,
    linewidth=2
)

ax[1].set_ylabel(
    "Accel (m/s2)"
)

ax[1].set_title(
    "Fallback Control Command"
)

# Travel distance
ax[2].plot(
    time_s,
    distance,
    linewidth=2
)

ax[2].set_ylabel(
    "Distance (m)"
)

# Command ownership
ax[3].step(
    time_s,
    source,
    where="post"
)

ax[3].set_yticks([0, 1])

ax[3].set_yticklabels(
    ["AI Stub", "Fallback"]
)

ax[3].set_xlabel(
    "Simulation Time (s)"
)

# Event markers
for a in ax:
    a.axvline(
        trigger,
        linestyle="--",
        label="Fallback Trigger"
    )

    a.axvline(
        stop_time,
        linestyle=":",
        label="Hold Stop"
    )

    a.grid(
        True,
        alpha=0.3
    )

ax[0].legend(
    loc="upper right"
)

fig.suptitle(
    "H-A-SEE Native Fallback Simulation"
)

plt.tight_layout()

output = csv_path.with_suffix(".png")

plt.savefig(
    output,
    dpi=200
)

print(
    "Generated:",
    output
)
