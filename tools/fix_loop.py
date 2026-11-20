"""Compile loop used with split_feature.py.

Builds the layer; for each "undeclared identifier" inside a member function of <Class> in <file.cpp>, puts `e.`
in front of that name, but only inside that class's member functions. For calls to moved <methods> in other files,
puts `<member>.` in front. Stops when it builds or nothing more can be fixed.

usage: fix_loop.py <file.cpp> <Class> <member> [methods...]
"""
import os
import re
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
layer_dir = os.path.join(here, '..', 'layer')
src_dir = os.path.join(layer_dir, 'src')
fname, cls, member = sys.argv[1:4]
methods = set(sys.argv[4:])
ERR = re.compile(r'[/\x5c](\w+\.(?:cpp|h))\((\d+)\): error (C\d+): (.*)')


def path(f):
    p = os.path.join(src_dir, f)
    return p if os.path.exists(p) else os.path.join(layer_dir, 'test', f)


def member_ranges(text):
    """(start, end) character ranges of the bodies of Class::Method definitions."""
    out = []
    for m in re.finditer(r'^[^\n]*\b' + cls + r'::\w+\([^;{]*\)[^;{]*\{', text, re.M):
        depth = 0
        for k in range(m.end() - 1, len(text)):
            if text[k] == '{':
                depth += 1
            elif text[k] == '}':
                depth -= 1
                if depth == 0:
                    out.append((m.end(), k))
                    break
    return out


def prefix_in_members(text, name):
    pat = re.compile(r'(?<![\w.>:])' + name + r'\b')
    pieces, last = [], 0
    for a, b in member_ranges(text):
        pieces.append(text[last:a])
        pieces.append(pat.sub('e.' + name, text[a:b]))
        last = b
    pieces.append(text[last:])
    return ''.join(pieces)


for it in range(40):
    bat = os.path.abspath(os.path.join(layer_dir, 'build.bat'))
    r = subprocess.run(['cmd', '/c', 'call', bat], cwd=os.path.abspath(layer_dir), capture_output=True, text=True)
    errs = ERR.findall(r.stdout + r.stderr)
    if not errs:
        print('builds after', it, 'rounds')
        break
    changed = False
    done = set()
    for f, line, code, msg in errs:
        m = re.search(r"'(\w+)'", msg)
        if not m:
            continue
        n = m.group(1)
        if (f, n) in done or n == 'e':
            continue
        done.add((f, n))
        p = path(f)
        t = open(p, encoding='utf-8').read()
        if f == fname and code in ('C2065', 'C3861'):
            t2 = prefix_in_members(t, n)
        elif n in methods and f != fname:
            t2 = re.sub(r'(?<![\w.>:])' + n + r'\(', member + '.' + n + '(', t)
        else:
            continue
        if t2 != t:
            open(p, 'w', encoding='utf-8').write(t2)
            changed = True
    if not changed:
        print('stuck:')
        print('\n'.join(f'{f}({l}): {c} {m}' for f, l, c, m in errs[:15]))
        break
