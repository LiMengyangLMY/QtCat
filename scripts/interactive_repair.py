"""Interactive Cat Rump Inpainting & Adjustment Tool.

Provides an interactive GUI for the user to:
1. Adjust rump fullness using a slider (from flat vertical cut to full plump rump).
2. Manually paint/erase the desired fill area on a 512x512 canvas.
3. Switch preview modes and background colors (checkerboard, pink, white, dark).
4. Save & compile directly to the desktop pet with a single button click.
"""

from __future__ import annotations

import os
import sys
import tkinter as tk
from tkinter import ttk, messagebox
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageTk

APP_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRAIN_DIR = r"C:\Users\13645\.gemini\antigravity\brain\dfec2217-65b3-4269-ab9c-24296b73ebd6"
ORIG_PATH = os.path.join(BRAIN_DIR, "scratch", "idle_hi.png")
FULL_PATH = os.path.join(BRAIN_DIR, "scratch", "gen", "idle_fixed_hi.png")
OUTPUT_PATH = os.path.join(APP_DIR, "resources", "assets", "idle", "idle.png")
DONE_FLAG = os.path.join(BRAIN_DIR, "scratch", "repair_done.flag")

CANVAS_SIZE = 512


def make_checkerboard(size: int, block: int = 16) -> Image.Image:
    arr = np.zeros((size, size, 3), dtype=np.uint8)
    for y in range(size):
        for x in range(size):
            if ((x // block) + (y // block)) % 2 == 0:
                arr[y, x] = (220, 220, 220)
            else:
                arr[y, x] = (255, 255, 255)
    return Image.fromarray(arr, "RGB")


class RepairApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("桌面宠物 - 猫咪屁股交互涂抹修整工具")
        self.root.geometry("820x680")
        self.root.resizable(False, False)

        # Load images
        self.orig_1024 = Image.open(ORIG_PATH).convert("RGBA")
        self.full_1024 = Image.open(FULL_PATH).convert("RGBA")

        # 512x512 representations for fast display and painting
        self.orig_512 = self.orig_1024.resize((CANVAS_SIZE, CANVAS_SIZE), Image.LANCZOS)
        self.full_512 = self.full_1024.resize((CANVAS_SIZE, CANVAS_SIZE), Image.LANCZOS)

        self.orig_a_512 = np.asarray(self.orig_512.getchannel("A"))
        self.full_a_512 = np.asarray(self.full_512.getchannel("A"))

        # Extra fillable region (where full has alpha, but orig does not)
        self.fillable_mask_512 = (self.full_a_512 > 40) & (self.orig_a_512 <= 40)

        # User mask (512x512, uint8: 0 or 255) - currently active fill area
        # Default initialize to moderate 50% rump
        self.user_mask_512 = np.zeros((CANVAS_SIZE, CANVAS_SIZE), dtype=np.uint8)
        self.apply_preset_pct(45)

        # Background templates
        self.bg_checker = make_checkerboard(CANVAS_SIZE)
        self.bg_pink = Image.new("RGB", (CANVAS_SIZE, CANVAS_SIZE), (255, 0, 255))
        self.bg_white = Image.new("RGB", (CANVAS_SIZE, CANVAS_SIZE), (245, 245, 245))
        self.bg_dark = Image.new("RGB", (CANVAS_SIZE, CANVAS_SIZE), (35, 35, 40))

        # Painting variables
        self.brush_size = tk.IntVar(value=18)
        self.bg_mode = tk.StringVar(value="checker")
        self.view_mode = tk.StringVar(value="composite")  # "composite" or "mask"
        self.slider_val = tk.IntVar(value=45)
        self.last_x = None
        self.last_y = None

        self._build_ui()
        self.update_canvas()

    def _build_ui(self):
        main_frame = ttk.Frame(self.root, padding=12)
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Left: Canvas area
        left_frame = ttk.Frame(main_frame)
        left_frame.pack(side=tk.LEFT, padx=(0, 14))

        self.canvas = tk.Canvas(
            left_frame,
            width=CANVAS_SIZE,
            height=CANVAS_SIZE,
            cursor="crosshair",
            highlightthickness=1,
            highlightbackground="#999999",
        )
        self.canvas.pack()

        self.canvas.bind("<ButtonPress-1>", lambda e: self.on_draw_start(e, mode="paint"))
        self.canvas.bind("<B1-Motion>", lambda e: self.on_draw_move(e, mode="paint"))
        self.canvas.bind("<ButtonRelease-1>", self.on_draw_end)

        self.canvas.bind("<ButtonPress-3>", lambda e: self.on_draw_start(e, mode="erase"))
        self.canvas.bind("<B3-Motion>", lambda e: self.on_draw_move(e, mode="erase"))
        self.canvas.bind("<ButtonRelease-3>", self.on_draw_end)

        tip_lbl = ttk.Label(
            left_frame,
            text="操作提示：左键拖拽 = 涂抹保留 | 右键拖拽 = 橡皮擦除",
            foreground="#555555",
            font=("Segoe UI", 9),
        )
        tip_lbl.pack(pady=(6, 0))

        # Right: Control Panel
        right_frame = ttk.Frame(main_frame)
        right_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

        # Section 1: Quick Size Slider
        sec1 = ttk.LabelFrame(right_frame, text=" 1. 快速大小滑块 (推荐先滑后涂) ", padding=10)
        sec1.pack(fill=tk.X, pady=(0, 10))

        slider_row = ttk.Frame(sec1)
        slider_row.pack(fill=tk.X, pady=4)
        ttk.Label(slider_row, text="屁股饱满度:").pack(side=tk.LEFT)
        self.slider_lbl = ttk.Label(slider_row, text="45%", width=6, anchor=tk.E)
        self.slider_lbl.pack(side=tk.RIGHT)

        self.scale = ttk.Scale(
            sec1,
            from_=0,
            to=100,
            orient=tk.HORIZONTAL,
            variable=self.slider_val,
            command=self.on_slider_change,
        )
        self.scale.pack(fill=tk.X, pady=(2, 6))

        btn_row = ttk.Frame(sec1)
        btn_row.pack(fill=tk.X)
        ttk.Button(btn_row, text="微补 (25%)", command=lambda: self.set_preset(25)).pack(
            side=tk.LEFT, expand=True, fill=tk.X, padx=2
        )
        ttk.Button(btn_row, text="适中 (45%)", command=lambda: self.set_preset(45)).pack(
            side=tk.LEFT, expand=True, fill=tk.X, padx=2
        )
        ttk.Button(btn_row, text="丰满 (75%)", command=lambda: self.set_preset(75)).pack(
            side=tk.LEFT, expand=True, fill=tk.X, padx=2
        )

        # Section 2: Brush Options
        sec2 = ttk.LabelFrame(right_frame, text=" 2. 手工微调涂抹 ", padding=10)
        sec2.pack(fill=tk.X, pady=(0, 10))

        brush_row = ttk.Frame(sec2)
        brush_row.pack(fill=tk.X, pady=4)
        ttk.Label(brush_row, text="画笔大小:").pack(side=tk.LEFT)
        self.brush_lbl = ttk.Label(brush_row, text=f"{self.brush_size.get()} px", width=6, anchor=tk.E)
        self.brush_lbl.pack(side=tk.RIGHT)

        brush_scale = ttk.Scale(
            sec2,
            from_=4,
            to=60,
            orient=tk.HORIZONTAL,
            variable=self.brush_size,
            command=lambda v: self.brush_lbl.config(text=f"{int(float(v))} px"),
        )
        brush_scale.pack(fill=tk.X, pady=2)

        view_row = ttk.Frame(sec2)
        view_row.pack(fill=tk.X, pady=4)
        ttk.Radiobutton(
            view_row,
            text="查看合成效果",
            variable=self.view_mode,
            value="composite",
            command=self.update_canvas,
        ).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Radiobutton(
            view_row,
            text="显示红色涂抹遮罩",
            variable=self.view_mode,
            value="mask",
            command=self.update_canvas,
        ).pack(side=tk.LEFT)

        # Section 3: Background Preview
        sec3 = ttk.LabelFrame(right_frame, text=" 3. 背景对比切换 ", padding=10)
        sec3.pack(fill=tk.X, pady=(0, 10))

        bg_row = ttk.Frame(sec3)
        bg_row.pack(fill=tk.X)
        for val, name in [
            ("checker", "棋盘格"),
            ("pink", "高对比粉色"),
            ("white", "白色"),
            ("dark", "深色"),
        ]:
            ttk.Radiobutton(
                bg_row,
                text=name,
                variable=self.bg_mode,
                value=val,
                command=self.update_canvas,
            ).pack(side=tk.LEFT, expand=True)

        # Section 4: Action Buttons
        sec4 = ttk.Frame(right_frame, padding=(0, 10))
        sec4.pack(fill=tk.X, side=tk.BOTTOM)

        apply_btn = tk.Button(
            sec4,
            text="✔ 满意，保存并应用到桌宠",
            font=("Segoe UI", 11, "bold"),
            bg="#2e7d32",
            fg="white",
            activebackground="#1b5e20",
            activeforeground="white",
            relief=tk.FLAT,
            height=2,
            command=self.apply_and_finish,
        )
        apply_btn.pack(fill=tk.X, pady=(0, 6))

        reset_btn = ttk.Button(sec4, text="重置为当前滑块值", command=self.reset_to_slider)
        reset_btn.pack(fill=tk.X)

    def set_preset(self, val: int):
        self.slider_val.set(val)
        self.on_slider_change(val)

    def on_slider_change(self, val):
        pct = int(float(val))
        self.slider_lbl.config(text=f"{pct}%")
        self.apply_preset_pct(pct)
        self.update_canvas()

    def reset_to_slider(self):
        self.apply_preset_pct(self.slider_val.get())
        self.update_canvas()

    def apply_preset_pct(self, pct: int):
        """Derive mask from full rump using erosion/cut logic."""
        if pct <= 0:
            self.user_mask_512[:] = 0
            return

        # Distance/erosion approach on full rump alpha
        full_a_img = Image.fromarray(self.full_a_512)
        # Map 100% -> radius 1 (no erosion), 0% -> radius 31
        radius = int(round(1 + (100 - pct) * 0.30))
        if radius % 2 == 0:
            radius += 1

        if radius <= 1:
            eroded = self.full_a_512
        else:
            e = full_a_img.filter(ImageFilter.MinFilter(radius))
            eroded = np.asarray(e)

        # In the fillable region, keep where eroded > 40
        self.user_mask_512[:] = np.where(self.fillable_mask_512 & (eroded > 40), 255, 0).astype(np.uint8)

    def on_draw_start(self, event, mode="paint"):
        self.last_x = event.x
        self.last_y = event.y
        self.draw_stroke(event.x, event.y, mode)

    def on_draw_move(self, event, mode="paint"):
        if self.last_x is not None and self.last_y is not None:
            self.draw_line(self.last_x, self.last_y, event.x, event.y, mode)
        self.last_x = event.x
        self.last_y = event.y

    def on_draw_end(self, event):
        self.last_x = None
        self.last_y = None
        self.update_canvas()

    def draw_stroke(self, x: int, y: int, mode="paint"):
        r = int(self.brush_size.get() // 2)
        mask_img = Image.fromarray(self.user_mask_512)
        draw = ImageDraw.Draw(mask_img)
        fill_val = 255 if mode == "paint" else 0
        draw.ellipse([x - r, y - r, x + r, y + r], fill=fill_val)
        # Only allow painting inside candidate fillable area + small margin
        self.user_mask_512 = np.asarray(mask_img)
        self.update_canvas()

    def draw_line(self, x0: int, y0: int, x1: int, y1: int, mode="paint"):
        w = int(self.brush_size.get())
        mask_img = Image.fromarray(self.user_mask_512)
        draw = ImageDraw.Draw(mask_img)
        fill_val = 255 if mode == "paint" else 0
        draw.line([x0, y0, x1, y1], fill=fill_val, width=w)
        r = w // 2
        draw.ellipse([x1 - r, y1 - r, x1 + r, y1 + r], fill=fill_val)
        self.user_mask_512 = np.asarray(mask_img)
        self.update_canvas()

    def render_composite(self) -> Image.Image:
        # Soften user mask
        m_img = Image.fromarray(self.user_mask_512).filter(ImageFilter.GaussianBlur(1.0))
        m_arr = np.asarray(m_img)

        # Alpha is orig_a + user_mask in candidate region
        combined_a = np.maximum(self.orig_a_512, m_arr)

        # Image composite
        full_arr = np.asarray(self.full_512)
        orig_arr = np.asarray(self.orig_512)

        is_orig = self.orig_a_512 > 10
        rgb = np.where(is_orig[..., None], orig_arr[..., :3], full_arr[..., :3])
        rgba = np.dstack([rgb, combined_a]).astype(np.uint8)
        cat_img = Image.fromarray(rgba, "RGBA")

        # Composite onto chosen background
        bg_name = self.bg_mode.get()
        if bg_name == "pink":
            base = self.bg_pink.copy()
        elif bg_name == "white":
            base = self.bg_white.copy()
        elif bg_name == "dark":
            base = self.bg_dark.copy()
        else:
            base = self.bg_checker.copy()

        if self.view_mode.get() == "mask":
            # Overlay semi-transparent red where mask is active
            red_overlay = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (255, 30, 30, 160))
            cat_on_base = base.convert("RGBA")
            cat_on_base.alpha_composite(cat_img)
            mask_overlay = Image.composite(red_overlay, Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0)), m_img)
            cat_on_base.alpha_composite(mask_overlay)
            return cat_on_base.convert("RGB")
        else:
            cat_on_base = base.convert("RGBA")
            cat_on_base.alpha_composite(cat_img)
            return cat_on_base.convert("RGB")

    def update_canvas(self):
        img = self.render_composite()
        self.tk_img = ImageTk.PhotoImage(img)
        self.canvas.create_image(0, 0, anchor=tk.NW, image=self.tk_img)

    def apply_and_finish(self):
        # Scale 512 mask up to 1024
        mask_512_img = Image.fromarray(self.user_mask_512)
        mask_1024_img = mask_512_img.resize((1024, 1024), Image.LANCZOS).filter(ImageFilter.GaussianBlur(1.2))
        m_arr_1024 = np.asarray(mask_1024_img)

        orig_1024_arr = np.asarray(self.orig_1024)
        full_1024_arr = np.asarray(self.full_1024)

        orig_a_1024 = orig_1024_arr[..., 3]
        combined_a_1024 = np.maximum(orig_a_1024, m_arr_1024)

        is_orig_1024 = orig_a_1024 > 10
        rgb_1024 = np.where(is_orig_1024[..., None], orig_1024_arr[..., :3], full_1024_arr[..., :3])
        rgba_1024 = np.dstack([rgb_1024, combined_a_1024]).astype(np.uint8)

        final_hi = Image.fromarray(rgba_1024, "RGBA")
        final_256 = final_hi.resize((256, 256), Image.LANCZOS)

        # Save to output path
        os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
        final_256.save(OUTPUT_PATH)

        # Write completion flag
        with open(DONE_FLAG, "w", encoding="utf-8") as f:
            f.write(f"slider={self.slider_val.get()}\n")

        messagebox.showinfo("成功", "素材已保存！点击确定后将关闭窗口，并自动完成应用编译。")
        self.root.destroy()


def main():
    root = tk.Tk()
    app = RepairApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
