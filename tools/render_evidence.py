"""Inspect linear PFM render artifacts; optional NumPy/Pillow are dev tools only."""
import argparse
import json
from pathlib import Path
import numpy as np

def load_pfm(path):
    with open(path, 'rb') as f:
        if f.readline().strip() != b'PF':
            raise ValueError('Expected RGB PFM')
        width, height = map(int, f.readline().split())
        scale = float(f.readline())
        pixels = np.frombuffer(f.read(), dtype='<f4' if scale < 0 else '>f4')
    if pixels.size != width * height * 3:
        raise ValueError('PFM length mismatch')
    return np.flipud(pixels.reshape(height, width, 3)).astype(np.float64) * abs(scale)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('image', type=Path)
    p.add_argument('--reference', type=Path)
    p.add_argument('--png', type=Path)
    p.add_argument('--exposure', type=float, default=1)
    p.add_argument('--json', type=Path)
    args = p.parse_args()
    image = load_pfm(args.image)
    if not np.isfinite(image).all() or (image < 0).any():
        raise ValueError('Nonfinite or negative radiance')
    report = {'image':str(args.image), 'width':image.shape[1], 'height':image.shape[0],
              'mean_rgb':image.mean(axis=(0,1)).tolist(), 'max':float(image.max()),
              'nonzero_fraction':float(np.mean(np.any(image > 0, axis=2)))}
    if args.reference:
        reference = load_pfm(args.reference)
        if image.shape != reference.shape:
            raise ValueError('Comparison requires identical dimensions')
        difference = image - reference
        report.update(reference=str(args.reference), linear_rmse=float(np.sqrt(np.mean(difference**2))),
                      linear_mae=float(np.mean(np.abs(difference))), reference_mean_rgb=reference.mean(axis=(0,1)).tolist())
    if args.png:
        from PIL import Image
        display = np.clip(1-np.exp(-image*args.exposure), 0, 1)**(1/2.2)
        args.png.parent.mkdir(parents=True, exist_ok=True)
        Image.fromarray(np.round(display*255).astype(np.uint8)).save(args.png)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
