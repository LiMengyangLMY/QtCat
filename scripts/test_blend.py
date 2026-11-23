import numpy as np
from PIL import Image, ImageFilter

A = r"C:\Users\13645\.gemini\antigravity\brain\dfec2217-65b3-4269-ab9c-24296b73ebd6"
orig_img = Image.open(A + r"\scratch\idle_hi.png").convert("RGBA")
gen_img = Image.open(A + r"\idle_green_fixed_v1_1795413493808.jpg").convert("RGB")

orig = np.asarray(orig_img).astype(np.float32)
gen = np.asarray(gen_img).astype(np.float32)

r, g, b = gen[..., 0], gen[..., 1], gen[..., 2]
greenness = g - np.maximum(r, b)
alpha = np.clip((120.0 - greenness) / (120.0 - 40.0), 0, 1)
g_fixed = np.minimum(g, np.maximum(r, b) + 6)
gen_rgba = np.dstack([r, g_fixed, b, alpha * 255])
a_img = Image.fromarray((alpha * 255).astype(np.uint8)).filter(ImageFilter.MedianFilter(3))
gen_rgba[..., 3] = np.asarray(a_img, dtype=np.float32)

# Full composite (100% rump)
H, W = orig.shape[:2]
x0, x1 = 420, 560
xs = np.arange(W, dtype=np.float32)
w = np.clip((x1 - xs) / (x1 - x0), 0, 1)[None, :].repeat(H, 0)
ys = np.arange(H, dtype=np.float32)[:, None]
w *= np.clip((760 - ys) / 60.0, 0, 1)
w = w[..., None]

def premul(x):
    return np.dstack([x[..., :3] * x[..., 3:4] / 255.0, x[..., 3:4]])

po, pg = premul(orig), premul(gen_rgba)
out = po * (1 - w) + pg * w
a = out[..., 3:4]
rgb = np.where(a > 0, out[..., :3] * 255.0 / np.maximum(a, 1e-3), 0)
full_comp = np.dstack([rgb, a]).clip(0, 255).astype(np.uint8)

print("Full composite shape:", full_comp.shape)
