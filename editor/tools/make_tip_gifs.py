"""Draws the animated tips for the Overview's "Getting started" box (Controls/TipsPanel.cs).

    python tools/make_tip_gifs.py             -> Assets/tips/<id>.gif (embedded in the editor on the next build)

Everything is drawn here (no outside art): a forearm with the wrist panel, Quest-style controllers that point, grip and
drag, a radial wheel. Drawn at 2x and scaled down for smooth edges. Colours match the editor's dark theme.
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont

W, H, S = 420, 250, 2          # output size, supersampling
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Assets", "tips")
FONT = ImageFont.truetype("C:/Windows/Fonts/seguisb.ttf", 15 * S)
FONT_S = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 11 * S)

BG = (24, 28, 35)
SLEEVE = (70, 82, 104)
SLEEVE_HI = (88, 102, 128)
SKIN = (196, 160, 132)
PANEL = (34, 40, 52)
PANEL_EDGE = (110, 130, 160)
BTN = (52, 64, 84)
BLUE = (74, 144, 240)
ORANGE = (240, 140, 30)
GREEN = (60, 190, 110)
CTRL = (215, 220, 228)
CTRL_DARK = (120, 128, 140)
TEXT = (235, 240, 248)
DIM = (150, 160, 176)


def lerp(a, b, t): return a + (b - a) * t
def ease(t): t = max(0.0, min(1.0, t)); return t * t * (3 - 2 * t)
def seg(f, a, b): return ease((f - a) / max(1, b - a))   # 0..1 between frames a and b


class Frame:
    def __init__(self):
        self.im = Image.new("RGB", (W * S, H * S), BG)
        self.d = ImageDraw.Draw(self.im)

    def P(self, x, y): return (x * S, y * S)

    def poly(self, pts, fill, outline=None, width=1):
        self.d.polygon([self.P(*p) for p in pts], fill=fill, outline=outline, width=width * S)

    def rrect(self, x0, y0, x1, y1, r, fill, outline=None, width=1):
        self.d.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], r * S, fill=fill, outline=outline, width=width * S)

    def circle(self, x, y, r, fill=None, outline=None, width=1):
        self.d.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=fill, outline=outline, width=width * S)

    def line(self, a, b, fill, width=1):
        self.d.line([self.P(*a), self.P(*b)], fill=fill, width=width * S)

    def text(self, x, y, s, fill=TEXT, font=FONT, anchor="mm"):
        self.d.text(self.P(x, y), s, fill=fill, font=font, anchor=anchor)

    def caption(self, s, step=None):
        self.rrect(10, H - 40, W - 10, H - 10, 8, (34, 40, 52))
        if step:
            self.circle(30, H - 25, 10, fill=ORANGE)
            self.text(30, H - 25, str(step), fill=(255, 255, 255), font=FONT_S)
            self.text(48, H - 25, s, anchor="lm")
        else:
            self.text(W / 2, H - 25, s)

    def done(self):
        return self.im.resize((W, H), Image.LANCZOS)


# ---- scene pieces ------------------------------------------------------------------------------------------------

def arm(f, y=78, lift=0.0):
    """Left forearm, elbow at the left edge, hand + left controller on the right. lift 0..1 turns it toward the viewer
    (drawn a little fatter and higher). Returns the wrist panel's centre line (x range, y)."""
    yy = y - lift * 6
    thick = 30 + lift * 8
    f.poly([(-20, yy - thick / 2 + 4), (250, yy - thick / 2), (262, yy - thick / 2 + 3), (262, yy + thick / 2 - 3),
            (250, yy + thick / 2), (-20, yy + thick / 2 + 6)], SLEEVE)
    f.poly([(-20, yy - thick / 2 + 4), (250, yy - thick / 2), (250, yy - thick / 2 + 6), (-20, yy - thick / 2 + 10)], SLEEVE_HI)
    # hand (fist round the controller)
    f.rrect(258, yy - 17, 290, yy + 17, 12, SKIN)
    controller(f, 300, yy - 4, ang=-15, scale=0.9, left=True)
    return yy


def panel(f, cx, cy, w=96, h=54, alpha=1.0, outline=PANEL_EDGE, glow=None, pressed=-1):
    """The wrist panel: 3 x 2 buttons. alpha < 1 squashes it (seen edge-on before the wrist turns)."""
    h2 = h * max(0.05, alpha)
    if glow:
        f.rrect(cx - w / 2 - 5, cy - h2 / 2 - 5, cx + w / 2 + 5, cy + h2 / 2 + 5, 10, None, glow, 3)
    f.rrect(cx - w / 2, cy - h2 / 2, cx + w / 2, cy + h2 / 2, 7, PANEL, outline, 2)
    if alpha > 0.5:
        bw, bh = (w - 16) / 3, (h2 - 14) / 2
        for r in range(2):
            for c in range(3):
                x0 = cx - w / 2 + 5 + c * (bw + 3)
                y0 = cy - h2 / 2 + 5 + r * (bh + 4)
                f.rrect(x0, y0, x0 + bw, y0 + bh, 3, ORANGE if pressed == r * 3 + c else BTN)


def controller(f, x, y, ang=0.0, scale=1.0, grip=False, trigger=False, left=False, stick=None, thumbrest=False):
    """A Quest-style controller seen from the side: tracking ring on top, handle below. (x, y) = the ring's centre.
    ang in degrees (0 = handle straight down). grip / trigger light the buttons orange."""
    a = math.radians(ang)
    ca, sa = math.cos(a), math.sin(a)
    def T(px, py): return (x + (px * ca - py * sa) * scale, y + (px * sa + py * ca) * scale)
    # handle
    f.poly([T(-11, 6), T(11, 6), T(9, 46), T(4, 52), T(-4, 52), T(-9, 46)], CTRL)
    # grip button on the handle's inner side
    gx = 1 if left else -1
    f.poly([T(gx * 11, 14), T(gx * 13, 18), T(gx * 12, 36), T(gx * 9, 38)], ORANGE if grip else CTRL_DARK)
    # trigger in front
    f.poly([T(-6, 4), T(6, 4), T(4, 12), T(-4, 12)], ORANGE if trigger else CTRL_DARK)
    # ring
    cx, cy = T(0, -10)
    f.circle(cx, cy, 20 * scale, outline=CTRL, width=max(2, int(6 * scale)))
    # face: thumbstick (+ where it is pushed) and thumbrest
    sx, sy = T(0, 0)
    if stick:
        f.line((sx, sy), (sx + stick[0] * 7, sy + stick[1] * 7), BLUE, 3)
        f.circle(sx + stick[0] * 7, sy + stick[1] * 7, 4 * scale, fill=BLUE)
    else:
        f.circle(sx, sy, 4 * scale, fill=CTRL_DARK)
    if thumbrest:
        tx, ty = T(7, 4)
        f.circle(tx, ty, 5 * scale, fill=ORANGE)
    return T(0, -29)  # top of the ring: where the laser starts


def laser(f, a, b, color=BLUE):
    f.line(a, b, color, 2)
    f.circle(b[0], b[1], 4, fill=color)


def label(f, x, y, s, color=ORANGE):
    tw = FONT_S.getlength(s) / S + 14
    f.rrect(x - tw / 2, y - 10, x + tw / 2, y + 10, 9, color)
    f.text(x, y, s, fill=(255, 255, 255), font=FONT_S)


# ---- the tips ----------------------------------------------------------------------------------------------------

def calibrate():
    frames = []
    N = 96
    for i in range(N):
        f = Frame()
        yy = arm(f)
        # panel position: starts at 190, dragged toward the elbow to 120, then grown
        drag = seg(i, 30, 56)
        grow = seg(i, 60, 72)
        px = lerp(190, 125, drag)
        pw, ph = lerp(96, 120, grow), lerp(54, 66, grow)
        gripping = 26 <= i < 80
        # right controller: comes in, then moves with the panel
        approach = seg(i, 0, 18)
        cx = lerp(380, px + 40, approach) if i < 30 else px + 40
        cy = lerp(175, 140, approach)
        stick = (0, -1) if 60 <= i < 72 else None
        if i >= 80:  # let go and move away
            back = seg(i, 80, 92)
            cx, cy = lerp(px + 40, 370, back), lerp(140, 165, back)
        glow = ORANGE if gripping else (BLUE if 12 <= i < 26 else None)
        panel(f, px, yy - 2, pw, ph, glow=glow)
        tip = controller(f, cx, cy, ang=25, grip=gripping, stick=stick)
        if 8 <= i < 80:
            laser(f, tip, (px + 10, yy + 4), ORANGE if gripping else BLUE)
        if gripping and i < 60:
            label(f, cx + 48, cy + 26, "holding grip")
        if 60 <= i < 72:
            label(f, cx + 60, cy + 26, "stick up / down = size", BLUE)
        if i < 26:
            f.caption("Calibrate: point at the panel", 1)
        elif i < 58:
            f.caption("Hold the grip and move your hand", 2)
        elif i < 78:
            f.caption("Thumbstick up / down: resize", 3)
        else:
            f.caption("Let go: it stays there (saved)", 4)
        frames.append(f.done())
    return frames


def show_panel():
    frames = []
    N = 84
    for i in range(N):
        f = Frame()
        turn = seg(i, 6, 26)
        yy = arm(f, lift=turn)
        panel(f, 170, yy - 2, alpha=turn, glow=GREEN if 26 <= i < 34 else None,
              pressed=4 if 52 <= i < 62 else -1)
        # eye looking at it
        ex, ey = 175, 22
        f.d.ellipse([(ex - 16) * S, (ey - 8) * S, (ex + 16) * S, (ey + 8) * S], outline=TEXT if turn > 0.9 else DIM, width=2 * S)
        f.circle(ex, ey + 1, 4, fill=TEXT if turn > 0.9 else DIM)
        if turn > 0.9:
            f.line((ex, ey + 10), (ex, yy - 32), (90, 100, 120), 1)
        # the other hand comes in and pokes (fingertip) a button
        come = seg(i, 34, 50)
        bx, by = 170 + 31, yy - 2 + 12   # button 4's centre
        cx, cy = lerp(385, bx + 26, come), lerp(165, by + 44, come)
        if i >= 62:
            back = seg(i, 62, 76)
            cx, cy = lerp(bx + 26, 385, back), lerp(by + 44, 165, back)
        tip = controller(f, cx, cy, ang=-30)
        if i >= 34:
            f.line(tip, (tip[0] - 6, tip[1] - 6), SKIN, 5)
            f.circle(tip[0] - 7, tip[1] - 7, 4, fill=SKIN)
        if i < 26:
            f.caption("Turn your forearm toward your face", 1)
        elif i < 34:
            f.caption("Look at it: the panel shows", 2)
        else:
            f.caption("Touch a button with your other hand", 3)
        frames.append(f.done())
    return frames


def fingertip():
    frames = []
    N = 84
    for i in range(N):
        f = Frame()
        # the poking (right) controller, still; its fingertip ball sits in front of the ring
        hx, hy = 270, 110
        tip = controller(f, hx, hy, ang=-25, scale=1.25)
        move = seg(i, 34, 56)
        fx, fy = lerp(tip[0] - 4, tip[0] - 34, move), lerp(tip[1] + 2, tip[1] - 10, move)
        held = 26 <= i < 62
        # the other (left) controller comes in and grabs the ball with its grip
        come = seg(i, 6, 24)
        gx, gy = lerp(40, fx - 14, come), lerp(150, fy + 30, come)
        if i >= 62:
            back = seg(i, 62, 76)
            gx, gy = lerp(fx - 14, 40, back), lerp(fy + 30, 150, back)
        controller(f, gx, gy, ang=35, grip=held, left=True, scale=1.1)
        f.circle(fx, fy, 9, fill=ORANGE if held else (255, 220, 170), outline=(255, 255, 255), width=2)
        f.text(fx + 2, fy - 22, "fingertip", font=FONT_S, fill=DIM)
        if held:
            label(f, gx - 10, min(gy + 72, H - 56), "holding grip")
        if i < 26:
            f.caption("Calibrating: a fingertip shows on each controller", 1)
        elif i < 62:
            f.caption("Grab it with your other hand's grip, move it", 2)
        else:
            f.caption("Let go: touches now land there", 3)
        frames.append(f.done())
    return frames


def wheels():
    frames = []
    N = 90
    names = ["Map", "Inventory", "Pause", "Screenshot", "Quick save", "More"]
    for i in range(N):
        f = Frame()
        cx, cy = 320, 130
        tap = (4 <= i < 9) or (13 <= i < 18)
        ltip = controller(f, cx, cy, ang=-20, scale=1.3, thumbrest=tap, trigger=56 <= i < 64)
        if tap:
            label(f, cx + 50, cy + 8, "tap")
        if i >= 20:
            o = seg(i, 20, 30)
            wx, wy, r = 150, 105, 70 * (0.6 + 0.4 * o)
            pick = 2  # "Pause"
            for k, n in enumerate(names):
                a0 = -90 + k * 60 - 30
                col = ORANGE if (k == pick and 56 <= i < 70) else (BLUE if (k == pick and i >= 36) else (52, 64, 84))
                f.d.pieslice([(wx - r) * S, (wy - r) * S, (wx + r) * S, (wy + r) * S], a0 + 1.5, a0 + 58.5, fill=col)
            f.circle(wx, wy, r * 0.38, fill=BG)
            f.text(wx, wy, "Right", font=FONT_S, fill=DIM)
            for k, n in enumerate(names):
                a = math.radians(-90 + k * 60)
                f.text(wx + math.cos(a) * r * 0.7, wy + math.sin(a) * r * 0.7, n, font=FONT_S)
            if i >= 32:
                a = math.radians(-90 + pick * 60)
                laser(f, ltip, (wx + math.cos(a) * r * 0.72, wy + math.sin(a) * r * 0.72 + 8))
        if 56 <= i < 64:
            label(f, cx + 50, cy + 30, "trigger")
        if i < 20:
            f.caption("Double-tap the thumb rest", 1)
        elif i < 50:
            f.caption("The wheel opens: point at an item", 2)
        else:
            f.caption("Pull the trigger to pick it", 3)
        frames.append(f.done())
    return frames


def save(name, frames, ms=55):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".gif")
    pal = [fr.convert("P", palette=Image.ADAPTIVE, colors=128) for fr in frames]
    pal[0].save(path, save_all=True, append_images=pal[1:], duration=ms, loop=0, disposal=1)
    print(path, len(frames), "frames", os.path.getsize(path) // 1024, "KB")


if __name__ == "__main__":
    save("calibrate", calibrate())
    save("show_panel", show_panel())
    save("fingertip", fingertip())
    save("wheels", wheels())
    import tip_gifs_more  # the per-button and general tips
    for name, fn in tip_gifs_more.ALL:
        save(name, fn())
