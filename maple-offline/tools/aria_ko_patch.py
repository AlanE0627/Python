#!/usr/bin/env python3
"""maple-offline: Korean String/Quest NX from a KMS client's Data/ img files.

Every string node of the v83 NX whose path also exists in the KMS data as a
string is replaced (same id-based paths). Quest dialogue (Say.img/<id>) is
only replaced when the KMS quest has exactly the same set of dialogue lines,
so a changed quest never gets mismatched lines. Skill per-level lines (h, hN)
keep the v83 text because the numbers differ between versions.

usage: aria_ko_patch.py string <String.nx> <out.nx> <Data/String dir>
       aria_ko_patch.py quest  <Quest.nx>  <out.nx> <Data/Quest dir>
The output is for the user's own device only; never commit it.
"""
import os, sys, shutil, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wzimg
from nx_ko_patch import NX, finish

KEY = wzimg.keystream(wzimg.IVS['KMS'], 0x40000)

def load_img(path):
    d = open(path, 'rb').read()
    return wzimg.parse_img(d, KEY)

def strings_of(nx, idx, prefix, out):
    name, first, cnt, typ = nx.node(idx)
    if typ == 3:
        out[prefix] = idx
    for c in range(first, first + cnt):
        strings_of(nx, c, prefix + (nx.string(nx.node(c)[0]),), out)

def aria_strings(node, prefix, out):
    for k, v in node.items():
        p = prefix + (k,)
        if v.type == 'string':
            out[p] = v.value
        if len(v):
            aria_strings(v, p, out)

SKILL_LEVEL = re.compile(r'^h\d*$')

def main():
    mode, src, dst, datadir = sys.argv[1:5]
    shutil.copyfile(src, dst)
    nx = NX(dst)
    new_strings, patches, counts = [], [], {}
    def put(idx, text, key):
        patches.append((idx, nx.sc + len(new_strings))); new_strings.append(text)
        counts[key] = counts.get(key, 0) + 1

    root = nx.children(0)
    if mode == 'string':
        for img, idx in root.items():
            p = os.path.join(datadir, img)
            if not os.path.exists(p):
                continue
            kv = {}; aria_strings(load_img(p), (), kv)
            mine = {}; strings_of(nx, idx, (), mine)
            for path, nidx in mine.items():
                if img == 'Skill.img' and SKILL_LEVEL.match(path[-1]):
                    continue
                t = kv.get(path)
                if t:
                    put(nidx, t, img)
    else:
        qdir = os.path.join(datadir, 'QuestData')
        aria = {}
        for f in os.listdir(qdir):
            if f.endswith('.img'):
                try:
                    aria[f[:-4]] = load_img(os.path.join(qdir, f))
                except Exception as e:
                    counts['parse_error'] = counts.get('parse_error', 0) + 1
        say_ok = say_skip = 0
        for img, sect in (('QuestInfo.img', 'QuestInfo'), ('Say.img', 'Say')):
            if img not in root:
                continue
            for qid, qidx in nx.children(root[img]).items():
                a = aria.get(qid)
                if a is None or sect not in a:
                    continue
                kv = {}; aria_strings(a[sect], (), kv)
                mine = {}; strings_of(nx, qidx, (), mine)
                mine.pop((), None)
                if sect == 'Say':
                    if set(mine) != set(kv):
                        say_skip += 1
                        continue
                    say_ok += 1
                for path, nidx in mine.items():
                    t = kv.get(path)
                    if t:
                        put(nidx, t, sect)
        counts['say_quests_replaced'] = say_ok
        counts['say_quests_kept_english'] = say_skip
    finish(nx, dst, new_strings, patches, counts)

if __name__ == '__main__':
    main()
