from pathlib import Path

from PIL import Image


root = Path(__file__).resolve().parents[1]
resources = root / "resources"


def save_resized(image, target, size):
    image = image.convert("RGB").resize((size, size), Image.Resampling.LANCZOS)
    options = {"quality": 95, "subsampling": 0} if target.suffix == ".jpg" else {"optimize": True}
    image.save(target, **options)
    print(f"Generated {target.relative_to(root)}: {size}x{size}")


# Keep the supplied masters intact. Runtime textures stay small so branding
# does not consume unnecessary Switch texture memory.
with Image.open(resources / "icon/nxnow-icon-master.png") as image:
    if image.width != image.height:
        raise ValueError("The application logo must be square")
    for name in ("nxnow-icon.png", "nxnow-icon.jpg", "icon.png", "icon.jpg"):
        save_resized(image, resources / "icon" / name, 256)
    save_resized(image, resources / "img/nxnow-logo-mark.png", 512)
    save_resized(image, resources / "img/nxnow-logo-mark-small.png", 128)

with Image.open(resources / "img/nxnow-logo-external.png") as image:
    if image.width != image.height:
        raise ValueError("The menu logo must be square")
    save_resized(image, resources / "img/nxnow-logo-external-small.png", 128)
