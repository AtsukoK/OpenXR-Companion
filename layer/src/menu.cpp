// The in-game menu's rows and layout (no drawing, no input): see menu.h.
#include "menu.h"
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

void MenuLayout(const MenuView& view, std::vector<MenuBox>& out) {
    out.clear();
    const float pad = 0.02f, headerH = kMenuHeaderH, footerH = kMenuFooterH;
    // header: back (when there is a page to go back to) and close
    if (view.canBack) out.push_back({ pad, pad, 0.16f, headerH - 2 * pad, { MenuPart::Back, -1 } });
    out.push_back({ 1 - pad - 0.12f, pad, 0.12f, headerH - 2 * pad, { MenuPart::Close, -1 } });
    // rows
    const float rowsTop = headerH, rowsH = 1 - headerH - footerH;
    const float rowH = rowsH / MenuView::kVisibleRows;
    int last = std::min((int)view.rows.size(), view.first + MenuView::kVisibleRows);
    for (int i = view.first; i < last; i++) {
        const MenuViewRow& r = view.rows[i];
        float y = rowsTop + (i - view.first) * rowH + 0.004f, h = rowH - 0.008f;
        switch (r.kind) {
        case MenuRowKind::Number:
        case MenuRowKind::Choice:
            out.push_back({ pad, y, 0.56f, h, { MenuPart::Row, i } });
            out.push_back({ 0.6f, y, 0.1f, h, { MenuPart::Less, i } });
            out.push_back({ 1 - pad - 0.1f, y, 0.1f, h, { MenuPart::More, i } });
            break;
        case MenuRowKind::Heading:
        case MenuRowKind::Note:
            out.push_back({ pad, y, 1 - 2 * pad, h, { MenuPart::None, i } }); // drawn, not pressable
            break;
        default:
            out.push_back({ pad, y, 1 - 2 * pad, h, { MenuPart::Row, i } });
            break;
        }
    }
    // scroll arrows when there is more than fits
    bool more = (int)view.rows.size() > MenuView::kVisibleRows;
    if (more) {
        float y = 1 - footerH + pad * 0.5f, h = footerH - pad;
        out.push_back({ pad, y, 0.14f, h, { MenuPart::ScrollUp, -1 } });
        out.push_back({ 1 - pad - 0.14f, y, 0.14f, h, { MenuPart::ScrollDown, -1 } });
    }
}
