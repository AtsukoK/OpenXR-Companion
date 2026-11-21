#include "keys.h"
#include <map>
#include <set>
#include <deque>
#include <thread>
#include <condition_variable>
#include <algorithm>

namespace Keys {

struct K {
    bool mouse = false;
    WORD vk = 0;
    bool ext = false;
    DWORD mDown = 0, mUp = 0, mData = 0;
    int wheel = 0;
};

static std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}
static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static const std::map<std::string, K>& Table() {
    static std::map<std::string, K> t = [] {
        std::map<std::string, K> m;
        auto k = [&](const char* n, WORD vk, bool ext = false) {
            K x;
            x.vk = vk;
            x.ext = ext;
            m[n] = x;
        };
        for (int i = 1; i <= 24; i++)
            k(("f" + std::to_string(i)).c_str(), (WORD)(VK_F1 + i - 1));
        for (char c = 'a'; c <= 'z'; c++)
            k(std::string(1, c).c_str(), (WORD)toupper(c));
        for (char c = '0'; c <= '9'; c++) {
            k(std::string(1, c).c_str(), (WORD)c);
            k(("num" + std::string(1, c)).c_str(), (WORD)(VK_NUMPAD0 + c - '0'));
        }
        k("space", VK_SPACE);
        k("enter", VK_RETURN);
        k("return", VK_RETURN);
        k("esc", VK_ESCAPE);
        k("escape", VK_ESCAPE);
        k("tab", VK_TAB);
        k("backspace", VK_BACK);
        k("delete", VK_DELETE, true);
        k("del", VK_DELETE, true);
        k("insert", VK_INSERT, true);
        k("ins", VK_INSERT, true);
        k("home", VK_HOME, true);
        k("end", VK_END, true);
        k("pageup", VK_PRIOR, true);
        k("pgup", VK_PRIOR, true);
        k("pagedown", VK_NEXT, true);
        k("pgdn", VK_NEXT, true);
        k("up", VK_UP, true);
        k("down", VK_DOWN, true);
        k("left", VK_LEFT, true);
        k("right", VK_RIGHT, true);
        k("ctrl", VK_LCONTROL);
        k("control", VK_LCONTROL);
        k("lctrl", VK_LCONTROL);
        k("rctrl", VK_RCONTROL, true);
        k("shift", VK_LSHIFT);
        k("lshift", VK_LSHIFT);
        k("rshift", VK_RSHIFT);
        k("alt", VK_LMENU);
        k("lalt", VK_LMENU);
        k("ralt", VK_RMENU, true);
        k("win", VK_LWIN, true);
        k("capslock", VK_CAPITAL);
        k("scrolllock", VK_SCROLL);
        k("pause", VK_PAUSE);
        k("printscreen", VK_SNAPSHOT, true);
        k("numlock", VK_NUMLOCK, true);
        k("numadd", VK_ADD);
        k("numsub", VK_SUBTRACT);
        k("nummul", VK_MULTIPLY);
        k("numdiv", VK_DIVIDE, true);
        k("numdec", VK_DECIMAL);
        k("numenter", VK_RETURN, true);
        k("minus", VK_OEM_MINUS);
        k("equals", VK_OEM_PLUS);
        k("comma", VK_OEM_COMMA);
        k("period", VK_OEM_PERIOD);
        k("slash", VK_OEM_2);
        k("grave", VK_OEM_3);
        k("tilde", VK_OEM_3);
        k("semicolon", VK_OEM_1);
        k("quote", VK_OEM_7);
        k("lbracket", VK_OEM_4);
        k("rbracket", VK_OEM_6);
        k("backslash", VK_OEM_5);
        auto mb = [&](const char* n, DWORD d, DWORD u, DWORD data = 0) {
            K x;
            x.mouse = true;
            x.mDown = d;
            x.mUp = u;
            x.mData = data;
            m[n] = x;
        };
        mb("lmb", MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP);
        mb("rmb", MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP);
        mb("mmb", MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP);
        mb("mouse4", MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP, XBUTTON1);
        mb("mouse5", MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP, XBUTTON2);
        K wu;
        wu.mouse = true;
        wu.wheel = WHEEL_DELTA;
        m["wheelup"] = wu;
        K wd;
        wd.mouse = true;
        wd.wheel = -WHEEL_DELTA;
        m["wheeldown"] = wd;
        return m;
    }();
    return t;
}

static bool Parse(const std::string& combo, std::vector<K>& out, std::string& err) {
    out.clear();
    std::string cur;
    std::vector<std::string> parts;
    for (char c : combo) {
        if (c == '+') {
            parts.push_back(Trim(cur));
            cur.clear();
        } else
            cur += c;
    }
    parts.push_back(Trim(cur));
    for (auto& p : parts) {
        auto it = Table().find(Lower(p));
        if (it == Table().end()) {
            err = "unknown key '" + p + "' in '" + combo + "'";
            return false;
        }
        out.push_back(it->second);
    }
    if (out.empty()) {
        err = "empty key combo";
        return false;
    }
    return true;
}

static bool gFocusGameBeforeKeys = false;
static std::set<std::string> gHeld; // combos currently held via Down/toggle

// Our own windows live in the game's process too (the headset mirror): never take them for the game.
static bool IsOurWindow(HWND h) {
    wchar_t cls[64] = {};
    GetClassNameW(h, cls, 64);
    return wcsncmp(cls, L"OpenXRCompanion", 15) == 0;
}

// The game's main window (visible, not owned by another window, not ours), or null.
static HWND FindGameWindow() {
    struct Search {
        DWORD pid;
        HWND found;
    } search{ GetCurrentProcessId(), nullptr };
    EnumWindows(
        [](HWND h, LPARAM lp) -> BOOL {
            auto* search = (Search*)lp;
            DWORD pid = 0;
            GetWindowThreadProcessId(h, &pid);
            if (pid != search->pid || !IsWindowVisible(h) || GetWindow(h, GW_OWNER) || IsOurWindow(h)) return TRUE;
            search->found = h;
            return FALSE;
        },
        (LPARAM)&search);
    return search.found;
}

static bool GameHasFocus() {
    HWND fg = GetForegroundWindow();
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(fg, &foregroundPid);
    return foregroundPid == GetCurrentProcessId() && !IsOurWindow(fg); // the mirror in front is not the game
}

static void FocusGameAlways() {
    if (GameHasFocus()) return;
    if (HWND game = FindGameWindow()) SetForegroundWindow(game);
}

// Only when the profile asks for it (SetFocusWindow).
static void FocusGame() {
    if (gFocusGameBeforeKeys) FocusGameAlways();
}

static void Send(const K& k, bool down) {
    INPUT in{};
    if (k.mouse) {
        in.type = INPUT_MOUSE;
        if (k.wheel) {
            if (!down) return;
            in.mi.dwFlags = MOUSEEVENTF_WHEEL;
            in.mi.mouseData = (DWORD)k.wheel;
        } else {
            in.mi.dwFlags = down ? k.mDown : k.mUp;
            in.mi.mouseData = k.mData;
        }
    } else {
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = (WORD)MapVirtualKeyW(k.vk, MAPVK_VK_TO_VSC);
        in.ki.dwFlags = KEYEVENTF_SCANCODE | (k.ext ? KEYEVENTF_EXTENDEDKEY : 0) | (down ? 0 : KEYEVENTF_KEYUP);
        if (k.vk == VK_PAUSE) {
            in.ki.wVk = k.vk;
            in.ki.dwFlags &= ~KEYEVENTF_SCANCODE;
        }
    }
    SendInput(1, &in, sizeof(in));
}
static void ComboDown(const std::vector<K>& ks) {
    for (auto& k : ks) {
        Send(k, true);
        Sleep(5);
    }
}
static void ComboUp(const std::vector<K>& ks) {
    for (auto it = ks.rbegin(); it != ks.rend(); ++it) {
        Send(*it, false);
        Sleep(5);
    }
}

// One background thread runs the queued input jobs in order, so waits never stall the game's frame.
// Started on first use, stopped by Shutdown().
class InputWorker {
public:
    void Post(std::function<void()> job) {
        {
            std::lock_guard l(mx);
            if (!thread.joinable()) {
                stopping = false;
                thread = std::thread([this] { Run(); });
            }
            jobs.push_back(std::move(job));
        }
        cv.notify_one();
    }
    // Runs the jobs still queued, then ends the thread.
    void Stop() {
        {
            std::lock_guard l(mx);
            stopping = true;
        }
        cv.notify_all();
        if (thread.joinable()) thread.join();
    }

private:
    void Run() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock l(mx);
                cv.wait(l, [this] { return stopping || !jobs.empty(); });
                if (stopping && jobs.empty()) return;
                job = std::move(jobs.front());
                jobs.pop_front();
            }
            job();
        }
    }
    std::mutex mx;
    std::condition_variable cv;
    std::deque<std::function<void()>> jobs;
    std::thread thread;
    bool stopping = false;
};
static InputWorker gWorker;

static void Post(std::function<void()> job) {
    gWorker.Post(std::move(job));
}

bool Validate(const std::string& combo, std::string& err) {
    std::vector<K> k;
    return Parse(combo, k, err);
}

void Tap(const std::string& combo, int holdMs) {
    std::vector<K> ks;
    std::string err;
    if (!Parse(combo, ks, err)) {
        Log("key: %s", err.c_str());
        return;
    }
    Post([ks, holdMs] {
        FocusGame();
        ComboDown(ks);
        Sleep(std::max(holdMs, 10));
        ComboUp(ks);
    });
}
void Down(const std::string& combo) {
    std::vector<K> ks;
    std::string err;
    if (!Parse(combo, ks, err)) {
        Log("key: %s", err.c_str());
        return;
    }
    gHeld.insert(combo);
    Post([ks] {
        FocusGame();
        ComboDown(ks);
    });
}
void Up(const std::string& combo) {
    std::vector<K> ks;
    std::string err;
    if (!Parse(combo, ks, err)) return;
    gHeld.erase(combo);
    Post([ks] { ComboUp(ks); });
}
void ReleaseAll() {
    auto held = gHeld;
    for (auto& c : held)
        Up(c);
}
void SetFocusWindow(bool on) {
    gFocusGameBeforeKeys = on;
}

static std::vector<std::string> SplitSteps(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ';' || c == '\n') {
            if (!Trim(cur).empty()) out.push_back(Trim(cur));
            cur.clear();
        } else
            cur += c;
    }
    if (!Trim(cur).empty()) out.push_back(Trim(cur));
    return out;
}

// One parsed step of a sequence: "wait 100", "down W", "up W" or a plain tap "F5".
struct SequenceStep {
    enum class Kind { Wait, Down, Up, Tap } kind = Kind::Tap;
    int waitMs = 0;
    std::vector<K> keys;
};

static bool ParseSequence(const std::string& steps, std::vector<SequenceStep>& out, std::string& err) {
    out.clear();
    for (auto& text : SplitSteps(steps)) {
        std::string lower = Lower(text);
        SequenceStep step;
        if (lower.rfind("wait ", 0) == 0) {
            step.kind = SequenceStep::Kind::Wait;
            step.waitMs = atoi(lower.c_str() + 5);
            if (step.waitMs <= 0) {
                err = "bad wait '" + text + "'";
                return false;
            }
            step.waitMs = std::min(step.waitMs, 10000);
        } else if (lower.rfind("down ", 0) == 0) {
            step.kind = SequenceStep::Kind::Down;
            if (!Parse(text.substr(5), step.keys, err)) return false;
        } else if (lower.rfind("up ", 0) == 0) {
            step.kind = SequenceStep::Kind::Up;
            if (!Parse(text.substr(3), step.keys, err)) return false;
        } else if (!Parse(text, step.keys, err)) {
            return false;
        }
        out.push_back(std::move(step));
    }
    return true;
}

bool ValidateSequence(const std::string& steps, std::string& err) {
    std::vector<SequenceStep> parsed;
    return ParseSequence(steps, parsed, err);
}

void Sequence(const std::string& steps) {
    std::vector<SequenceStep> parsed;
    std::string err;
    if (!ParseSequence(steps, parsed, err)) {
        Log("sequence: %s", err.c_str());
        return;
    }
    Post([parsed] {
        FocusGame();
        for (auto& step : parsed) {
            switch (step.kind) {
            case SequenceStep::Kind::Wait:
                Sleep(step.waitMs);
                break;
            case SequenceStep::Kind::Down:
                ComboDown(step.keys);
                break;
            case SequenceStep::Kind::Up:
                ComboUp(step.keys);
                break;
            case SequenceStep::Kind::Tap:
                ComboDown(step.keys);
                Sleep(50);
                ComboUp(step.keys);
                break;
            }
        }
    });
}

// Tests: mouse calls are recorded here instead of moving the real mouse.
struct DryRun {
    bool isOn = false;
    int x = -1, y = -1;
    bool left = false, right = false;
    int wheel = 0, focusCount = 0;
};
static DryRun gDry;

void SetDryRun(bool on) {
    gDry = DryRun{};
    gDry.isOn = on;
}
void DryState(int& x, int& y, bool& l, bool& r, int& wheel, int& focus) {
    x = gDry.x;
    y = gDry.y;
    l = gDry.left;
    r = gDry.right;
    wheel = gDry.wheel;
    focus = gDry.focusCount;
}

void MouseTo(int x, int y) {
    if (gDry.isOn) {
        gDry.x = x;
        gDry.y = y;
        return;
    }
    Post([x, y] {
        // Per-monitor aware for this call so x, y are real pixels whatever the game's DPI setting.
        using SetCtx = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
        static auto set = (SetCtx)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext");
        DPI_AWARENESS_CONTEXT old = set ? set(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) : nullptr;
        SetCursorPos(x, y);
        if (set && old) set(old);
    });
}
void MouseButton(int b, bool down) {
    if (gDry.isOn) {
        if (b == 0)
            gDry.left = down;
        else if (b == 1)
            gDry.right = down;
        return;
    }
    Post([b, down] {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = b == 0   ? (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP)
                        : b == 1 ? (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
                                 : (down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
        SendInput(1, &in, sizeof(in));
    });
}
void MouseWheel(int delta) {
    if (gDry.isOn) {
        gDry.wheel += delta;
        return;
    }
    Post([delta] {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_WHEEL;
        in.mi.mouseData = (DWORD)delta;
        SendInput(1, &in, sizeof(in));
    });
}
void BringToFront(uintptr_t hwnd) {
    if (gDry.isOn) return;
    Post([hwnd] {
        HWND h = (HWND)hwnd;
        if (!IsWindow(h) || GetForegroundWindow() == h) return;
        SetForegroundWindow(h);
        Sleep(30); // let it come up before the click lands
    });
}
void FocusGameNow() {
    if (gDry.isOn) {
        gDry.focusCount++;
        return;
    }
    Post([] { FocusGameAlways(); });
}

void Shutdown() {
    ReleaseAll();
    gWorker.Stop();
}
} // namespace Keys
