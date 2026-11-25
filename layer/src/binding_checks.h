#pragma once
#include <string>
#include <vector>
#include "config.h"

// "May need your attention" in the headset: the same checks as the desktop editor (core/BindingChecks.cs), for one
// binding. Kept short: the headset shows a title and one line.
struct BindingWarning {
    std::string title, text;
};
std::vector<BindingWarning> CheckBinding(const Profile& p, int index, bool vigemInstalled);
