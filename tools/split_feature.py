"""Moves one feature's Engine methods + run state into its own class (used once per feature during the refactor).

usage: python split_feature.py <file.cpp> <ClassName> <engineMember> <stateStruct> <stateVar> <header.h> "<doc>" [public methods to forward...]

- every `Engine::X(` defined in <file.cpp> becomes `ClassName::X(`; their declarations move from engine.h to <header.h>
- the nested `struct <stateStruct> {...} <stateVar>;` in engine.h moves into the class as `run_`
- names the compiler then can't find in <file.cpp> are Engine members: they get `e.` in front
- calls to moved methods elsewhere get `<engineMember>.` in front
- the listed public methods keep a forwarding Engine method
"""
import re, subprocess, sys, os

src_dir = os.path.join(os.path.dirname(__file__), '..', 'layer', 'src')
layer_dir = os.path.join(os.path.dirname(__file__), '..', 'layer')
fname, cls, member, state_struct, state_var, header, doc = sys.argv[1:8]
forward = sys.argv[8:]
P = lambda f: os.path.join(src_dir, f)
read = lambda f: open(P(f), encoding='utf-8').read()
def write(f, s): open(P(f), 'w', encoding='utf-8').write(s)

cpp = read(fname)
eh = read('engine.h')
methods = sorted(set(re.findall(r'\bEngine::(\w+)\(', cpp)))

# 1. declarations out of engine.h
decls = []
for m in methods:
    pat = re.compile(r'^[ \t]*(?:static )?[\w:<>&*, ]+?[ &*]' + m + r'\([^;{]*\)(?: const)?;[^\n]*\n', re.M)
    found = pat.search(eh)
    if not found: sys.exit('no declaration for ' + m)
    decls.append(found.group(0).strip())
    eh = eh[:found.start()] + eh[found.end():]

# 2. the state struct out of engine.h
sp = re.search(r'^[ \t]*struct ' + state_struct + r' \{', eh, re.M)
depth, i = 0, sp.start()
j = eh.index('{', i)
for k in range(j, len(eh)):
    if eh[k] == '{': depth += 1
    elif eh[k] == '}':
        depth -= 1
        if depth == 0: break
end = eh.index(';', k) + 1
state_text = eh[sp.start():k + 1].strip() + ';'
state_text = '\n'.join(l[4:] if l.startswith('    ') else l for l in state_text.split('\n'))
eh = eh[:sp.start()] + eh[end + 1:]

# forwarders keep their Engine declaration
fwd_decls = [d for d in decls if re.search(r'\b(' + '|'.join(forward) + r')\(', d)] if forward else []
for d in fwd_decls:
    eh = eh.replace('public:\n', 'public:\n    ' + d + '\n', 1)

# 3. member + friend in Engine
eh = eh.replace('class Engine {\n', 'class Engine {\n    friend class ' + cls + ';\n', 1)
eh = re.sub(r'(\nprivate:\n)', r'\1    ' + cls + ' ' + member + '{ *this };\n', eh, count=1)
eh = eh.replace('#include "config.h"\n', '#include "config.h"\n#include "' + header + '"\n', 1)
write('engine.h', eh)

# 4. the new header
indent = lambda t: '\n'.join(('    ' + l if l.strip() else l) for l in t.split('\n'))
hdr = ('#pragma once\n// ' + doc + '\n#include "config.h"\n\nclass Engine;\n\nclass ' + cls + ' {\npublic:\n'
       '    explicit ' + cls + '(Engine& engine) : e(engine) {}\n\n'
       + '\n'.join('    ' + d for d in decls) + '\n\nprivate:\n    friend class Engine;\n    Engine& e;\n\n'
       + indent(state_text.replace(state_struct + ' {', 'Run {', 1).rstrip(';').rstrip() + ' run_;') + '\n};\n')
write(header, hdr)

# 5. the .cpp
cpp = re.sub(r'\bEngine::(\w+)\(', lambda m: cls + '::' + m.group(1) + '(' if m.group(1) in methods else m.group(0), cpp)
cpp = re.sub(r'(?<![\w.>])' + state_var + r'\.', 'run_.', cpp)
write(fname, cpp)

# forwarders
eng = read('engine.cpp')
for d in fwd_decls:
    sig = re.match(r'(.*?)\b(\w+)\((.*)\)( const)?;', d)
    ret, name, params, const = sig.group(1).strip(), sig.group(2), sig.group(3), sig.group(4) or ''
    args = ', '.join(p.strip().split()[-1].lstrip('&*') for p in params.split(',') if p.strip())
    eng += '\n' + ret + ' Engine::' + name + '(' + params + ')' + const + ' {\n    ' + ('' if ret == 'void' else 'return ') + member + '.' + name + '(' + args + ');\n}\n'
write('engine.cpp', eng)

print('moved', len(methods), 'methods; now run fix_loop.py')
