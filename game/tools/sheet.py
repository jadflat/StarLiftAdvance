"""Join simulator snaps into one sheet: python tools/sheet.py <dir> name1 name2 ... (5 a row)."""
import sys
from PIL import Image
d, names = sys.argv[1], sys.argv[2:]
ims = [Image.open(f"{d}/{n}.png") for n in names]
w, h = ims[0].size
s = Image.new('RGB', (w * min(5, len(ims)), h * ((len(ims) + 4) // 5)))
for i, im in enumerate(ims):
    s.paste(im, ((i % 5) * w, (i // 5) * h))
s.save(f"{d}/sheet.png")
