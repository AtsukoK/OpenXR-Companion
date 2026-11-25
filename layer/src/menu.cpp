// The in-game menu's rows and layout (no drawing, no input): see menu.h.
#include "menu.h"
#include "engine.h"
#include "controller_silhouettes.h"
#include <algorithm>
#include <cmath>

MenuRow MenuRow::Heading(std::string label) {
    MenuRow r;
    r.kind = MenuRowKind::Heading;
    r.label = std::move(label);
    return r;
}

MenuRow MenuRow::Note(std::string label, std::string detail) {
    MenuRow r;
    r.kind = MenuRowKind::Note;
    r.label = std::move(label);
    r.detail = std::move(detail);
    return r;
}

MenuRow MenuRow::Link(std::string label, std::string detail, std::function<void()> open) {
    MenuRow r;
    r.kind = MenuRowKind::Link;
    r.label = std::move(label);
    r.detail = std::move(detail);
    r.press = std::move(open);
    return r;
}

MenuRow MenuRow::Button(std::string label, std::string detail, std::function<void()> press) {
    MenuRow r = Link(std::move(label), std::move(detail), std::move(press));
    r.kind = MenuRowKind::Button;
    return r;
}

MenuRow MenuRow::Toggle(std::string label, std::string detail, std::function<bool()> isOn, std::function<void(bool)> setOn) {
    MenuRow r;
    r.kind = MenuRowKind::Toggle;
    r.label = std::move(label);
    r.detail = std::move(detail);
    r.isOn = std::move(isOn);
    r.setOn = std::move(setOn);
    return r;
}

MenuRow MenuRow::Number(std::string label, std::string detail, std::function<float()> value, std::function<void(float)> set, float min,
                        float max, float step) {
    MenuRow r;
    r.kind = MenuRowKind::Number;
    r.label = std::move(label);
    r.detail = std::move(detail);
    r.value = std::move(value);
    r.set = std::move(set);
    r.min = min;
    r.max = max;
    r.step = step;
    return r;
}

MenuRow MenuRow::Choice(std::string label, std::string detail, std::vector<std::string> options, std::function<int()> chosen,
                        std::function<void(int)> choose) {
    MenuRow r;
    r.kind = MenuRowKind::Choice;
    r.label = std::move(label);
    r.detail = std::move(detail);
    r.options = std::move(options);
    r.chosen = std::move(chosen);
    r.choose = std::move(choose);
    return r;
}

MenuRow MenuRow::Live(LiveKind what, int hand, int span) {
    MenuRow r;
    r.kind = MenuRowKind::Live;
    r.live = what;
    r.liveHand = hand;
    r.span = span;
    return r;
}

MenuRow MenuRow::Overview(std::vector<MenuTag> tags, std::function<void(int, int)> open, int span) {
    MenuRow r;
    r.kind = MenuRowKind::Overview;
    r.tags = std::move(tags);
    r.openTag = std::move(open);
    r.span = span;
    return r;
}

MenuRow MenuRow::Bar(std::vector<std::string> items, std::vector<std::function<void()>> presses) {
    MenuRow r;
    r.kind = MenuRowKind::Bar;
    r.items = std::move(items);
    r.itemPress = std::move(presses);
    return r;
}

MenuRow MenuRow::Tiles(std::vector<std::string> titles, std::vector<std::string> details, std::vector<std::function<void()>> presses,
                       int cols, int span) {
    MenuRow r;
    r.kind = MenuRowKind::Tiles;
    r.items = std::move(titles);
    r.itemDetails = std::move(details);
    r.itemPress = std::move(presses);
    r.cols = std::max(1, cols);
    r.span = std::max(1, span);
    return r;
}

MenuRow MenuRow::Preview(Wheel w, bool wrist, int cols, std::function<void(int)> openItem, std::vector<std::string> side,
                         std::vector<std::function<void()>> sidePress, int span) {
    MenuRow r;
    r.kind = MenuRowKind::Preview;
    r.previews.push_back(std::move(w));
    r.previewWrist = wrist;
    r.previewCols = cols;
    r.openItem = std::move(openItem);
    r.items = std::move(side);
    r.itemPress = std::move(sidePress);
    r.span = std::max(2, span);
    return r;
}

void PreviewLayout(const MenuView& view, const MenuViewRow& row, int rowIndex, float y, float h, PreviewPlaces& out) {
    out = PreviewPlaces{};
    const float pad = 0.02f, aspect = view.h > 0 ? view.w / view.h : 1.f;
    float side = h * 0.96f, picW = side / aspect; // square on the panel
    out.pic = { pad, y + h * 0.02f, picW, side, { MenuPart::None, rowIndex } };
    int n = row.PreviewCount();
    if (!row.previewWrist) {
        const float kPi = 3.14159265f, R = 0.49f, ri = R * 0.34f + 0.01f, tr = (R * 0.97f + ri) / 2, box = (R - ri) * 0.7f;
        float seg = n ? 2 * kPi / n : 0;
        for (int i = 0; i < n; i++) {
            float a = -kPi / 2 + i * seg, cx = 0.5f + std::cos(a) * tr, cy = 0.5f + std::sin(a) * tr;
            out.items.push_back({ out.pic.x + (cx - box / 2) * picW, out.pic.y + (cy - box / 2) * side, box * picW, box * side,
                                  { MenuPart::Row, rowIndex, i } });
        }
    } else {
        std::vector<Cell> cells;
        Cell head;
        WristLayout(n, row.previewCols, cells, head);
        for (int i = 0; i < (int)cells.size(); i++)
            out.items.push_back({ out.pic.x + cells[i].x * picW, out.pic.y + cells[i].y * side, cells[i].w * picW, cells[i].h * side,
                                  { MenuPart::Row, rowIndex, i } });
    }
    int k = (int)row.items.size();
    float sx = out.pic.x + picW + 0.03f, sw = 1 - pad - sx;
    float bh = std::min(0.1f, (h - 0.02f * (k + 1)) / std::max(1, k));
    float top = y + (h - (bh * k + 0.02f * (k - 1))) / 2;
    for (int i = 0; i < k; i++)
        out.side.push_back({ sx, top + i * (bh + 0.02f), sw, bh, { MenuPart::Row, rowIndex, n + i } });
}

bool LiveHand::operator==(const LiveHand& o) const {
    auto isClose = [](float a, float b) { return std::fabs(a - b) < 0.01f; }; // tiny tremble doesn't redraw
    for (int c = 0; c < C_COUNT; c++)
        if (press[c] != o.press[c] || touch[c] != o.touch[c]) return false;
    return kind == o.kind && isClose(trigger, o.trigger) && isClose(grip, o.grip) && isClose(stickX, o.stickX) && isClose(stickY, o.stickY) &&
           shape.innerDeadzone == o.shape.innerDeadzone && shape.outerDeadzone == o.shape.outerDeadzone &&
           shape.directionAngleDeg == o.shape.directionAngleDeg && shape.ignoreOutside == o.shape.ignoreOutside &&
           shape.ignoreCenter == o.shape.ignoreCenter;
}

int MenuMaxFirst(const MenuView& view) {
    int slots = 0, first = (int)view.rows.size();
    while (first > 0 && slots + view.rows[first - 1].span <= MenuView::kVisibleRows) {
        first--;
        slots += view.rows[first].span;
    }
    return first;
}

std::string MenuRow::ValueText() const {
    switch (kind) {
    case MenuRowKind::Toggle:
        return isOn && isOn() ? "ON" : "OFF";
    case MenuRowKind::Number: {
        float v = value ? value() : 0;
        if (!atMin.empty() && v <= min + step * 0.01f) return atMin;
        char b[48];
        snprintf(b, sizeof(b), "%.*f%s%s", decimals, v * scale, unit.empty() ? "" : " ", unit.c_str());
        return b;
    }
    case MenuRowKind::Choice: {
        int i = chosen ? chosen() : -1;
        return i >= 0 && i < (int)options.size() ? options[i] : "?";
    }
    case MenuRowKind::Link:
        return "\xE2\x80\xBA"; // ›
    default:
        return "";
    }
}

float MenuSquareScale(const MenuView& view) {
    return view.w > 0 && view.h > 0 ? (kMenuWidth / kMenuHeight) / (view.w / view.h) : 1.f;
}

bool PartCentre(ControllerKind kind, int hand, int comp, bool side, const MenuBox& pic, float aspect, float& u, float& v) {
    // the picture's design units fitted into the box, keeping their shape (the box is in u, v; u is aspect times longer)
    auto fit = [&](float designW, float x, float y) {
        float s = std::min(pic.w * aspect / designW, pic.h / kArtHeight);
        float ox = pic.x * aspect + (pic.w * aspect - designW * s) / 2, oy = pic.y + (pic.h - kArtHeight * s) / 2;
        u = (ox + x * s) / aspect;
        v = oy + y * s;
    };
    if (const Silhouette* sil = SilhouetteFor(kind, hand)) {
        for (int i = 0; i < sil->partCount; i++) {
            const SilPart& p = sil->parts[i];
            if (p.side != side || SilhouetteComp(p.comp) != comp || p.loopCount == 0) continue;
            float sx = 0, sy = 0;
            for (int k = 0; k < p.loops[0].points; k++) {
                sx += p.loops[0].xy[2 * k];
                sy += p.loops[0].xy[2 * k + 1];
            }
            fit(side ? kSilSideWidth : kArtWidth, sx / p.loops[0].points, sy / p.loops[0].points);
            return true;
        }
        return false;
    }
    if (side) return false; // the simple drawings are from above only
    for (const ArtPart& a : ArtFor(kind).parts) {
        if (a.comp != comp || !(a.hands & (hand == 0 ? 1 : 2))) continue;
        float x = a.x + a.w / 2;
        fit(kArtWidth, hand == 0 ? kArtWidth - x : x, a.y + a.h / 2); // the left hand is drawn mirrored
        return true;
    }
    return false;
}

// Each hand gets half the row: [tags] [from above] [from the side] [tags]. A tag goes to the column on the side of
// its part, the columns are kept about even, and the tags in a column are stacked in the order of their parts.
void OverviewLayout(const MenuView& view, const MenuViewRow& row, float y, float h, OverviewPlaces& out) {
    out = OverviewPlaces{};
    const float aspect = view.h > 0 ? view.w / view.h : 1.f;
    const float lineH = 0.03f, gap = 0.012f, captionH = 0.045f;
    for (int hand = 0; hand < 2; hand++) {
        // the two hands with a clear gap between them, so the middle tags don't run into each other
        float x0 = hand == 0 ? 0.005f : 0.517f;
        const float colW = 0.105f, topW = 0.15f, sideW = 0.1f, gapU = 0.006f;
        float picTop = y + captionH, picH = h - captionH - 0.01f;
        out.caption[hand] = { x0, y + 0.005f, 0.48f, captionH, {} };
        out.top[hand] = { x0 + colW + gapU, picTop, topW, picH, {} };
        out.side[hand] = { out.top[hand].x + topW + gapU, picTop, sideW, picH, {} };
        const ControllerKind kind = view.live[hand].kind;
        struct Item {
            int tag;
            float ax, ay, h;
            bool right;
        };
        std::vector<Item> items;
        for (int t = 0; t < (int)row.tags.size(); t++) {
            const MenuTag& tag = row.tags[t];
            if (tag.hand != hand) continue;
            float ax = -1, ay = -1;
            bool onSide = false;
            if (!PartCentre(kind, hand, tag.comp, false, out.top[hand], aspect, ax, ay)) {
                onSide = PartCentre(kind, hand, tag.comp, true, out.side[hand], aspect, ax, ay);
                if (!onSide) ax = ay = -1;
            }
            float th = lineH * (1.25f + std::min<size_t>(tag.lines.size(), 5)) + 0.012f;
            bool right = onSide || ax < 0 || ax >= out.top[hand].x + out.top[hand].w / 2;
            items.push_back({ t, ax, ay < 0 ? 9.f : ay, th, right });
        }
        // keep the columns about even: move the leftmost parts from above over
        auto count = [&](bool right) { return (int)std::count_if(items.begin(), items.end(), [&](const Item& i) { return i.right == right; }); };
        while (count(true) > count(false) + 1) {
            Item* move = nullptr;
            for (Item& i : items)
                if (i.right && i.ax >= 0 && i.ax < out.side[hand].x && (!move || i.ax < move->ax)) move = &i;
            if (!move) break;
            move->right = false;
        }
        for (int col = 0; col < 2; col++) {
            std::vector<Item*> list;
            for (Item& i : items)
                if (i.right == (col == 1)) list.push_back(&i);
            std::sort(list.begin(), list.end(), [](const Item* a, const Item* b) { return a->ay < b->ay; });
            float cx = col == 0 ? x0 : out.side[hand].x + sideW + gapU;
            // stacked from the top, each as near its part as the ones above allow; then pushed up if it overflows
            std::vector<float> ys;
            float next = picTop;
            for (Item* i : list) {
                float want = i->ay < 9.f ? i->ay - i->h / 2 : next;
                float at = std::max(next, want);
                ys.push_back(at);
                next = at + i->h + gap;
            }
            float overflow = next - gap - (y + h);
            for (int k = (int)ys.size() - 1; k >= 0 && overflow > 0; k--) {
                float limit = k == 0 ? picTop : ys[k - 1] + list[k - 1]->h + gap;
                float shift = std::min(overflow, ys[k] - limit);
                for (int j = k; j < (int)ys.size(); j++)
                    ys[j] -= shift > 0 ? shift : 0;
                overflow = ys.back() + list.back()->h - (y + h);
            }
            for (size_t k = 0; k < list.size(); k++) {
                OverviewTagPlace pl;
                pl.box = { cx, ys[k], colW, list[k]->h, { MenuPart::Row, -1, list[k]->tag } };
                pl.ax = list[k]->ax;
                pl.ay = list[k]->ay < 9.f ? list[k]->ay : -1;
                pl.tag = list[k]->tag;
                out.tags.push_back(pl);
            }
        }
    }
}

void MenuLayout(const MenuView& view, std::vector<MenuBox>& out) {
    out.clear();
    const float pad = 0.02f, headerH = kMenuHeaderH, footerH = kMenuFooterH;
    const float sq = MenuSquareScale(view); // square parts keep their shape on a wider panel
    // header: back (when there is a page to go back to) and close, square icon buttons
    if (view.canBack) out.push_back({ pad * sq, pad * 1.5f, 0.1f * sq, headerH - 3 * pad, { MenuPart::Back, -1 } });
    out.push_back({ 1 - (pad + 0.1f) * sq, pad * 1.5f, 0.1f * sq, headerH - 3 * pad, { MenuPart::Close, -1 } });
    // rows
    const float rowsTop = headerH, rowsH = 1 - headerH - footerH;
    const float rowH = rowsH / MenuView::kVisibleRows;
    int slot = 0;
    for (int i = view.first; i < (int)view.rows.size(); i++) {
        const MenuViewRow& r = view.rows[i];
        if (slot + r.span > MenuView::kVisibleRows) break;
        float y = rowsTop + slot * rowH, h = rowH * r.span;
        slot += r.span;
        switch (r.kind) {
        case MenuRowKind::Number:
        case MenuRowKind::Choice:
            out.push_back({ 0, y, 0.58f, h, { MenuPart::Row, i } });
            out.push_back({ 0.6f, y + h * 0.2f, 0.08f * sq, h * 0.6f, { MenuPart::Less, i } });
            out.push_back({ 1 - (pad + 0.08f) * sq, y + h * 0.2f, 0.08f * sq, h * 0.6f, { MenuPart::More, i } });
            break;
        case MenuRowKind::Heading:
        case MenuRowKind::Note:
        case MenuRowKind::Live:
            out.push_back({ 0, y, 1, h, { MenuPart::None, i } }); // drawn, not pressable
            break;
        case MenuRowKind::Overview: {
            out.push_back({ 0, y, 1, h, { MenuPart::None, i } }); // the pictures
            OverviewPlaces places;
            OverviewLayout(view, r, y, h, places);
            for (const OverviewTagPlace& t : places.tags)
                out.push_back({ t.box.x, t.box.y, t.box.w, t.box.h, { MenuPart::Row, i, t.tag } });
            break;
        }
        case MenuRowKind::Bar: {
            int n = std::max<int>(1, (int)r.items.size());
            float gapU = 0.015f, bw = (1 - 2 * pad - gapU * (n - 1)) / n;
            for (int k = 0; k < (int)r.items.size(); k++)
                out.push_back({ pad + k * (bw + gapU), y + h * 0.12f, bw, h * 0.76f, { MenuPart::Row, i, k } });
            break;
        }
        case MenuRowKind::Preview: {
            PreviewPlaces pl;
            PreviewLayout(view, r, i, y, h, pl);
            out.push_back(pl.pic);
            for (const MenuBox& b : pl.items)
                out.push_back(b);
            for (const MenuBox& b : pl.side)
                out.push_back(b);
            break;
        }
        case MenuRowKind::Tiles: {
            int n = (int)r.items.size(), cols = std::max(1, r.cols), lines = std::max(1, (n + cols - 1) / cols);
            float gapU = 0.02f, gapV = 0.025f;
            float tw = (1 - 2 * pad - gapU * (cols - 1)) / cols, th = (h - gapV * (lines + 1)) / lines;
            for (int k = 0; k < n; k++)
                out.push_back({ pad + (k % cols) * (tw + gapU), y + gapV + (k / cols) * (th + gapV), tw, th, { MenuPart::Row, i, k } });
            break;
        }
        default:
            out.push_back({ 0, y, 1, h, { MenuPart::Row, i } });
            break;
        }
    }
    // scroll arrows when there is more than fits
    bool more = MenuMaxFirst(view) > 0;
    if (more) {
        float y = 1 - footerH + pad * 0.5f, h = footerH - pad;
        out.push_back({ 1 - (pad + 0.18f) * sq, y, 0.08f * sq, h, { MenuPart::ScrollUp, -1 } });
        out.push_back({ 1 - (pad + 0.08f) * sq, y, 0.08f * sq, h, { MenuPart::ScrollDown, -1 } });
    }
}
