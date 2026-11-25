"""More tip GIFs: one per default wrist button kind, and the general tips. Uses the drawing helpers of make_tip_gifs.py,
which runs these too (python tools/make_tip_gifs.py [names...])."""
import math
from PIL import ImageFont
from make_tip_gifs import (Frame, S, H, seg, lerp, arm, panel, controller, laser, label,
                           PANEL, PANEL_EDGE, BTN, BLUE, ORANGE, GREEN, SKIN, DIM, FONT_S)

FONT_T = ImageFont.truetype("C:/Windows/Fonts/seguisb.ttf", 8 * S)


def lpanel(f, cx, cy, labels, w=150, h=80, lit=-1, title=None):
    """A wrist panel page with labelled buttons (3 columns). Returns the centre of each button."""
    f.rrect(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2, 7, PANEL, PANEL_EDGE, 2)
    top = cy - h / 2 + 5
    if title:
        f.text(cx, top + 5, title, fill=BLUE, font=FONT_T)
        top += 13
    rows = (len(labels) + 2) // 3
    bw = (w - 16) / 3
    bh = (cy + h / 2 - 5 - top - (rows - 1) * 4) / rows
    centres = []
    for k, lab in enumerate(labels):
        r, c = divmod(k, 3)
        x0 = cx - w / 2 + 5 + c * (bw + 3)
        y0 = top + r * (bh + 4)
        f.rrect(x0, y0, x0 + bw, y0 + bh, 3, ORANGE if k == lit else BTN)
        f.text(x0 + bw / 2, y0 + bh / 2, lab, font=FONT_T)
        centres.append((x0 + bw / 2, y0 + bh / 2))
    return centres


def finger_controller(f, cx, cy):
    tip = controller(f, cx, cy, ang=-30)
    f.line(tip, (tip[0] - 6, tip[1] - 6), SKIN, 5)
    f.circle(tip[0] - 7, tip[1] - 7, 4, fill=SKIN)


def presser(f, i, target, t_in=(6, 20), t_out=(30, 42), home=(470, 190)):
    """The other hand comes in and touches target with its fingertip, then leaves."""
    bx, by = target
    come = seg(i, *t_in)
    cx, cy = lerp(home[0], bx + 26, come), lerp(home[1], by + 44, come)
    if i >= t_out[0]:
        back = seg(i, *t_out)
        cx, cy = lerp(bx + 26, home[0], back), lerp(by + 44, home[1], back)
    finger_controller(f, cx, cy)


def arm_panel(f, labels, lit=-1, title=None, cx=150, w=150, h=80):
    yy = arm(f, y=70)
    return lpanel(f, cx, yy - 2, labels, w=w, h=h, lit=lit, title=title)


PAGE1 = ["Quick save", "Pause", "Screen", "Keyboard", "Config ->"]
PAGE2 = ["Quick load", "Reload", "Calibrate", "Offsets", "VR mirror", "Settings"]


def arrow(f, x0, y, x1, o):
    if o <= 0: return
    f.line((x0, y), (x0 + (x1 - x0) * o, y), ORANGE, 3)
    if o >= 1:
        f.d.polygon([f.P(x1, y - 8), f.P(x1, y + 8), f.P(x1 + 12, y)], fill=ORANGE)


def gif_key():
    frames = []
    for i in range(80):
        f = Frame()
        press = 20 <= i < 28
        c = arm_panel(f, PAGE1, lit=0 if press else -1)
        presser(f, i, c[0])
        if i >= 24:
            kx, ky = 345, 130
            f.rrect(kx - 24, ky - 20 + (3 if press else 0), kx + 24, ky + 20, 6, (60, 66, 78), (140, 150, 170), 2)
            f.text(kx, ky + (2 if press else 0), "F5")
            f.text(kx, ky + 32, "keyboard key", font=FONT_S, fill=DIM)
            if i >= 32:
                label(f, kx, 182, "Game saved", GREEN)
        f.caption("Press a button on the panel" if i < 24 else "The game gets the key (F5 here)", 1 if i < 24 else 2)
        frames.append(f.done())
    return frames


def gif_page():
    frames = []
    for i in range(80):
        f = Frame()
        press = 20 <= i < 28
        flip = seg(i, 28, 38)
        yy = arm(f, y=70)
        c = None
        if flip < 1:
            c = lpanel(f, 150, yy - 2 - flip * 50, PAGE1, lit=4 if press else -1, title="Page 1")
        if flip > 0:
            lpanel(f, 150, yy - 2 + (1 - flip) * 50, PAGE2, title="Page 2: Configuration")
        if c is not None and i < 28:
            presser(f, i, c[4], t_out=(28, 38))
        f.caption("Press 'Config ->' (Configuration)" if i < 30 else "The panel shows its next page", 1 if i < 30 else 2)
        frames.append(f.done())
    return frames


def gif_screen():
    frames = []
    for i in range(90):
        f = Frame()
        press = 20 <= i < 28
        c = arm_panel(f, PAGE1, lit=2 if press else -1, cx=110, w=130)
        presser(f, i, c[2])
        f.rrect(330, 112, 412, 162, 5, (40, 46, 58), (120, 130, 150), 2)
        f.rrect(337, 119, 380, 135, 3, (70, 110, 200))
        f.rrect(337, 139, 405, 156, 3, (60, 70, 90))
        f.text(371, 174, "your monitor", font=FONT_S, fill=DIM)
        o = seg(i, 30, 42)
        if o > 0:
            sx, sy, sw = 200, 150, 140 * o
            f.rrect(sx - sw / 2, sy - sw * 0.28, sx + sw / 2, sy + sw * 0.28, 5, (40, 46, 58), ORANGE, 2)
            if o > 0.8:
                f.rrect(sx - 62, sy - 32, sx - 8, sy - 12, 3, (70, 110, 200))
                f.rrect(sx - 62, sy - 8, sx + 62, sy + 30, 3, (60, 70, 90))
        if i >= 50:
            tip = controller(f, 300, 190, ang=-40, trigger=62 <= i < 68)
            laser(f, tip, (220, 160))
            f.d.polygon([f.P(220, 160), f.P(220, 173), f.P(228, 168)], fill=(255, 255, 255))
        f.caption("Press 'Screen Mirror'" if i < 30 else ("Your monitor shows on your arm" if i < 50 else "Point at it to use the mouse"),
                  1 if i < 30 else (2 if i < 50 else 3))
        frames.append(f.done())
    return frames


KEYS = "qwertyuiopasdfghjklzxcvbnm"


def key_pos(k, kx, ky):
    r = 0 if k < 10 else (1 if k < 19 else 2)
    col = k if r == 0 else (k - 10 if r == 1 else k - 19)
    return kx - 100 + r * 8 + col * 20, ky - 28 + r * 20


def gif_keyboard():
    frames = []
    word = "hello"
    for i in range(96):
        f = Frame()
        press = 18 <= i < 26
        c = arm_panel(f, PAGE1, lit=3 if press else -1, cx=95, w=120)
        presser(f, i, c[3], t_in=(4, 18), t_out=(26, 36))
        o = seg(i, 26, 36)
        if o > 0:
            kx, ky = 285, 130
            f.rrect(kx - 110 * o, ky - 40 * o, kx + 110 * o, ky + 40 * o, 6, PANEL, PANEL_EDGE, 2)
            if o > 0.9:
                typed = min(len(word), max(0, (i - 40) // 9))
                pressing = (i - 40) % 9 < 4
                for k, ch in enumerate(KEYS):
                    x, y = key_pos(k, kx, ky)
                    hit = 1 <= typed and ch == word[typed - 1] and pressing
                    f.rrect(x, y, x + 17, y + 16, 3, ORANGE if hit else BTN)
                    f.text(x + 8.5, y + 8, ch, font=FONT_T)
                f.rrect(kx - 80, 22, kx + 80, 46, 5, (40, 46, 58), (120, 130, 150), 1)
                f.text(kx - 72, 34, word[:typed] + ("|" if i % 10 < 5 else ""), anchor="lm", font=FONT_S)
                tip = controller(f, 395, 190, ang=-35, trigger=typed >= 1 and pressing)
                if typed >= 1:
                    x, y = key_pos(KEYS.index(word[typed - 1]), kx, ky)
                    laser(f, tip, (x + 8, y + 8))
        f.caption("Press 'Open VR Keyboard'" if i < 30 else "Point at keys and pull the trigger to type", 1 if i < 30 else 2)
        frames.append(f.done())
    return frames


def gun(f, x, y, ang):
    a = math.radians(ang)
    ca, sa = math.cos(a), math.sin(a)
    def T(px, py): return (x + px * ca - py * sa, y + px * sa + py * ca)
    f.poly([T(-10, -6), T(60, -6), T(60, 4), T(-10, 4)], (90, 96, 110))
    f.poly([T(-6, 4), T(8, 4), T(4, 26), T(-8, 26)], (150, 110, 80))


def gif_offsets():
    frames = []
    for i in range(96):
        f = Frame()
        fix = seg(i, 34, 66)
        err = lerp(28, 0, fix)
        cx, cy = 120, 110
        y0 = cy - 34
        f.line((cx, y0), (410, y0), (90, 100, 120), 1)
        f.text(345, y0 - 12, "where your hand aims", font=FONT_S, fill=DIM)
        if err > 2:
            a = math.radians(err)
            f.line((cx + 10, y0), (cx + 10 + math.cos(a) * 280, y0 + math.sin(a) * 280), (220, 80, 80), 2)
            f.text(345, y0 + math.sin(a) * 215 + 16, "where the game aims", font=FONT_S, fill=(220, 120, 120))
        else:
            label(f, 345, y0 + 22, "matches", GREEN)
        gun(f, cx + 10, y0, err)
        if 26 <= i < 70:
            controller(f, cx + 80, cy + 60, ang=-20, grip=True, left=True)
            label(f, cx + 140, cy + 84, "grab and turn")
        f.caption("The game's gun points lower than your hand" if i < 30 else ("Controller offsets: grab and turn it" if i < 70 else "Saved for that game"),
                  1 if i < 30 else (2 if i < 70 else 3))
        frames.append(f.done())
    return frames


def view(f, x, y, w, h):
    f.rrect(x, y, x + w, y + h, 4, (46, 60, 84))
    f.rrect(x, y + h * 0.6, x + w, y + h, 4, (52, 70, 52))
    f.circle(x + w * 0.35, y + h * 0.45, h * 0.22, fill=(52, 64, 84), outline=BLUE, width=1)
    f.rrect(x + w * 0.62, y + h * 0.55, x + w * 0.85, y + h * 0.78, 2, PANEL, ORANGE, 1)


def gif_headset_mirror():
    frames = []
    for i in range(80):
        f = Frame()
        f.rrect(40, 60, 150, 120, 18, (60, 66, 78), (140, 150, 170), 2)
        f.rrect(52, 72, 92, 108, 8, (24, 28, 35))
        f.rrect(98, 72, 138, 108, 8, (24, 28, 35))
        if i >= 30:
            view(f, 54, 74, 36, 32)
            view(f, 100, 74, 36, 32)
        f.text(95, 138, "your headset", font=FONT_S, fill=DIM)
        o = seg(i, 14, 30)
        arrow(f, 160, 90, 232, o)
        f.rrect(250, 30, 400, 140, 6, (40, 46, 58), (120, 130, 150), 2)
        f.rrect(315, 140, 335, 156, 1, (120, 130, 150))
        if o >= 1:
            view(f, 262, 42, 126, 86)
        f.text(325, 172, "a desktop window (OBS can record it)", font=FONT_S, fill=DIM)
        f.caption("Press 'VR mirror to Desktop'" if i < 30 else "What you see shows in a desktop window", 1 if i < 30 else 2)
        frames.append(f.done())
    return frames


def gif_settings_menu():
    frames = []
    for i in range(90):
        f = Frame()
        press = 18 <= i < 26
        c = arm_panel(f, PAGE2, lit=5 if press else -1, cx=95, w=130, h=86, title="Configuration")
        presser(f, i, c[5], t_in=(4, 18), t_out=(26, 36))
        o = seg(i, 26, 36)
        if o > 0:
            mx, my = 290, 105
            f.rrect(mx - 95 * o, my - 85 * o, mx + 95 * o, my + 85 * o, 8, PANEL, PANEL_EDGE, 2)
            if o > 0.9:
                f.text(mx, my - 70, "Settings", font=FONT_S)
                on = i >= 60
                for k, (lab, val) in enumerate([("Wheels", True), ("Wrist panels", True), ("Vibration", on), ("Notifications", True)]):
                    y = my - 45 + k * 26
                    f.rrect(mx - 85, y - 10, mx + 85, y + 10, 4, (40, 46, 58))
                    f.text(mx - 78, y, lab, anchor="lm", font=FONT_S)
                    f.rrect(mx + 48, y - 7, mx + 76, y + 7, 7, GREEN if val else (80, 86, 98))
                    f.circle(mx + (69 if val else 55), y, 5, fill=(255, 255, 255))
                tip = controller(f, 400, 175, ang=-35, trigger=56 <= i < 62)
                if i >= 44:
                    laser(f, tip, (mx + 62, my - 45 + 2 * 26))
        f.caption("Press 'Companion overlay Settings'" if i < 30 else "Change settings right in the headset", 1 if i < 30 else 2)
        frames.append(f.done())
    return frames


def gif_close():
    frames = []
    for i in range(72):
        f = Frame()
        yy = arm(f, y=70)
        hide = seg(i, 28, 36)
        back = seg(i, 52, 60)
        a = max(0.0, min(1.0, 1 - hide + back))
        if a > 0.9:
            c = lpanel(f, 150, yy - 2, PAGE1 + ["Hide"], lit=5 if 20 <= i < 28 else -1)
            if i < 28:
                presser(f, i, c[5], t_out=(26, 36))
        elif a > 0.05:
            panel(f, 150, yy - 2, 150, 80, alpha=a)
        if 36 <= i < 52:
            label(f, 300, 130, "look away", BLUE)
        f.caption("Press 'Hide'" if i < 30 else ("It stays hidden..." if i < 52 else "...until you look at your arm again"),
                  1 if i < 30 else (2 if i < 52 else 3))
        frames.append(f.done())
    return frames


def gif_reload():
    frames = []
    for i in range(70):
        f = Frame()
        press = 18 <= i < 26
        c = arm_panel(f, PAGE2, lit=1 if press else -1, cx=150, w=150, h=86, title="Configuration")
        presser(f, i, c[1], t_in=(4, 18), t_out=(26, 36))
        if i >= 26:
            # a file being read again
            fx, fy = 330, 130
            f.poly([(fx - 22, fy - 28), (fx + 12, fy - 28), (fx + 22, fy - 18), (fx + 22, fy + 28), (fx - 22, fy + 28)], (60, 66, 78), (140, 150, 170), 2)
            f.text(fx, fy, "profile", font=FONT_T)
            a = (i - 26) * 20
            for k in range(3):
                ang = math.radians(a + k * 120)
                f.circle(fx + math.cos(ang) * 38, fy + math.sin(ang) * 38, 4, fill=BLUE)
        f.caption("Press 'Reload profile'" if i < 30 else "It reads your saved profile again", 1 if i < 30 else 2)
        frames.append(f.done())
    return frames


def gif_how_press():
    frames = []
    names = ["Point + trigger", "Touch", "Either"]
    for i in range(108):
        f = Frame()
        mode = (i // 36) % 3
        k = i % 36
        for m, n in enumerate(names):
            x = 20 + m * 132
            f.rrect(x, 8, x + 120, 30, 8, ORANGE if m == mode else (40, 46, 58))
            f.text(x + 60, 19, n, font=FONT_S)
        c = lpanel(f, 150, 120, ["A", "B", "C", "D", "E", "F"], w=140, h=80, lit=4 if 16 <= k < 24 else -1)
        tgt = c[4]
        if mode == 0 or (mode == 2 and k < 18):
            tip = controller(f, 360, 170, ang=-40, trigger=16 <= k < 24)
            laser(f, tip, tgt)
        else:
            come = seg(k, 0, 14) if mode == 1 else seg(k, 18, 26)
            finger_controller(f, lerp(380, tgt[0] + 26, come), lerp(170, tgt[1] + 44, come))
        f.caption(["Point the laser, pull the trigger", "Touch the button with your fingertip", "Either way works"][mode])
        frames.append(f.done())
    return frames


def gif_save():
    frames = []
    for i in range(90):
        f = Frame()
        f.rrect(14, 14, 210, 160, 6, (34, 40, 52), (90, 100, 120), 2)
        f.rrect(14, 14, 210, 32, 6, (21, 24, 28))
        clicked = 22 <= i < 30
        f.rrect(24, 18, 100, 30, 3, ORANGE if clicked else (52, 64, 84))
        f.text(62, 24, "Save (Ctrl+S)", font=FONT_T)
        newlab = "Map" if i >= 8 else "Quick load"
        f.text(30, 50, "Button label:", anchor="lm", font=FONT_S, fill=DIM)
        f.rrect(30, 60, 190, 80, 3, (24, 28, 35), BLUE if 4 <= i < 20 else (80, 90, 110), 1)
        f.text(36, 70, newlab + ("|" if 4 <= i < 20 and i % 8 < 4 else ""), anchor="lm", font=FONT_S)
        mx, my = (60, 26) if i >= 18 else (150, 100)
        f.d.polygon([f.P(mx, my), f.P(mx, my + 14), f.P(mx + 9, my + 10)], fill=(255, 255, 255))
        o = seg(i, 30, 40)
        arrow(f, 218, 90, 262, o)
        if o >= 1:
            label(f, 240, 116, "within a second", BLUE)
        lpanel(f, 345, 90, ["Map" if i >= 40 else "Quick load", "Reload", "Calibrate", "Offsets", "Mirror", "Settings"], w=120, h=76,
               lit=0 if 40 <= i < 50 else -1, title="in the headset")
        f.caption("Change something, press Save" if i < 32 else "The headset shows it, no restart", 1 if i < 32 else 2)
        frames.append(f.done())
    return frames


ALL = [("key", gif_key), ("page", gif_page), ("screen", gif_screen), ("keyboard", gif_keyboard),
       ("offsets", gif_offsets), ("headset_mirror", gif_headset_mirror), ("settings_menu", gif_settings_menu),
       ("close", gif_close), ("reload", gif_reload), ("how_press", gif_how_press), ("save", gif_save)]
