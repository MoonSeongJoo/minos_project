from PIL import Image
from pathlib import Path

src = Path("demo/demo_frames_src")
dst = Path("demo/demo_frames_rpi4_640")
dst.mkdir(parents=True, exist_ok=True)

for i in range(100):
    inp = src / f"frame_{i:04d}.jpg"
    out = dst / f"frame_{i:04d}.jpg"

    img = Image.open(inp).convert("RGB")
    img = img.resize((640, 360))
    img.save(out, quality=85)

print("done")