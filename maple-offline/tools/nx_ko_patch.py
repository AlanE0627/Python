#!/usr/bin/env python3
"""maple-offline: build a Korean String.nx from the v83 String.nx.

Replaces item / map / mob / NPC names (and item descriptions) with the KMS
names from the localisation JSON files, by id. Strings without a Korean
counterpart stay as they are. The NX file is patched in place on a copy:
new strings are appended and a new string offset table is written at the
end, then the header is updated.

usage: nx_ko_patch.py <String.nx> <out String_ko.nx> <i18n dir>
       nx_ko_patch.py --quest <Quest.nx> <out Quest_ko.nx> <i18n dir>

Skill names / descriptions (Skill.img) are also replaced; per-level skill
lines (h1..hN) stay as they are, since later KMS numbers differ from v83.
With --quest, QuestInfo.img quest names are replaced in Quest.nx.
"""
import json
import mmap
import os
import shutil
import struct
import sys


class NX:
    def __init__(self, path):
        self.f = open(path, 'r+b')
        self.m = mmap.mmap(self.f.fileno(), 0)
        assert self.m[:4] == b'PKG4'
        (self.nc, self.no, self.sc, self.so,
         self.bc, self.bo, self.ac, self.ao) = struct.unpack_from('<IQIQIQIQ', self.m, 4)

    def node(self, i):
        return struct.unpack_from('<IIHH', self.m, self.no + i * 20)

    def string(self, i):
        off = struct.unpack_from('<Q', self.m, self.so + i * 8)[0]
        n = struct.unpack_from('<H', self.m, off)[0]
        return self.m[off + 2:off + 2 + n].decode('utf8', 'replace')

    def children(self, i):
        _, first, cnt, _ = self.node(i)
        return {self.string(self.node(c)[0]): c for c in range(first, first + cnt)}

    def child(self, i, name):
        return self.children(i).get(name) if i is not None else None


def load(dirpath, kind):
    """id -> record, KMS 284 first (closest to v83 era), 389 fills gaps."""
    out = {}

    for ver in ('389', '284'):
        p = os.path.join(dirpath, 'KMS_%s_%s.json' % (ver, kind))

        if os.path.exists(p):
            for r in json.load(open(p, encoding='utf8')):
                out[int(r['id'])] = r

    return out


def overrides_main(src, dst, path):
    """Second pass: set names from a hand-made table {npc|mob: {id: name}, map: {id: {name, streetName}}}."""
    shutil.copyfile(src, dst)
    nx = NX(dst)
    ov = json.load(open(path, encoding='utf8'))
    new_strings, patches, counts = [], [], {}

    def set_string(node_idx, text):
        if node_idx is None or not text or nx.node(node_idx)[3] != 3:
            return False
        patches.append((node_idx, nx.sc + len(new_strings)))
        new_strings.append(text)
        return True

    for img, key in (('Npc.img', 'npc'), ('Mob.img', 'mob')):
        base = nx.child(0, img)
        table = ov.get(key, {})
        for k, c in (nx.children(base).items() if base is not None else []):
            if k in table and set_string(nx.child(c, 'name'), table[k]):
                counts[key] = counts.get(key, 0) + 1
    items = ov.get('item', {})

    def item_node(c, k):
        if k in items and set_string(nx.child(c, 'name'), items[k]):
            counts['item'] = counts.get('item', 0) + 1

    for img in ('Consume.img', 'Ins.img', 'Cash.img', 'Pet.img'):
        base = nx.child(0, img)
        for k, c in (nx.children(base).items() if base is not None else []):
            item_node(c, k)
    etc = nx.child(nx.child(0, 'Etc.img'), 'Etc')
    for k, c in (nx.children(etc).items() if etc is not None else []):
        item_node(c, k)
    eqp = nx.child(nx.child(0, 'Eqp.img'), 'Eqp')
    for cat, cnode in (nx.children(eqp).items() if eqp is not None else []):
        for k, c in nx.children(cnode).items():
            item_node(c, k)
    mapimg = nx.child(0, 'Map.img')
    table = ov.get('map', {})
    for region, rnode in (nx.children(mapimg).items() if mapimg is not None else []):
        for k, c in nx.children(rnode).items():
            if k in table:
                if set_string(nx.child(c, 'mapName'), table[k].get('name')):
                    counts['map'] = counts.get('map', 0) + 1
                set_string(nx.child(c, 'streetName'), table[k].get('streetName'))
    finish(nx, dst, new_strings, patches, counts)


def main():
    if sys.argv[1] == '--overrides':
        return overrides_main(*sys.argv[2:5])
    quest_mode = sys.argv[1] == '--quest'
    args = sys.argv[2:5] if quest_mode else sys.argv[1:4]
    src, dst, i18n = args
    shutil.copyfile(src, dst)
    nx = NX(dst)

    items = load(i18n, 'item') if not quest_mode else {}
    maps = load(i18n, 'map')
    mobs = load(i18n, 'mob')
    npcs = load(i18n, 'npc')
    skills = load(i18n, 'skill')

    # hand-made names for v83 skills that later KMS data no longer has
    table = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ko_skill_names.json')

    if os.path.exists(table):
        for r in json.load(open(table, encoding='utf8')):
            skills.setdefault(int(r['id']), {'name': r['name']})
    quests = load(i18n, 'quest')

    new_strings = []          # appended strings
    patches = []              # (node index, new string id)

    def set_string(node_idx, text):
        if node_idx is None or not text:
            return False

        _, _, _, typ = nx.node(node_idx)

        if typ != 3:          # string nodes only
            return False

        patches.append((node_idx, nx.sc + len(new_strings)))
        new_strings.append(text)
        return True

    counts = {}

    def item_node(node, rid):
        r = items.get(rid)

        if not r:
            return

        if set_string(nx.child(node, 'name'), r.get('name')):
            counts['item'] = counts.get('item', 0) + 1

        desc = r.get('desc')

        if desc:
            set_string(nx.child(node, 'desc'), desc)

    root = 0

    if quest_mode:
        info = nx.child(root, 'QuestInfo.img')

        for k, c in (nx.children(info).items() if info is not None else []):
            if k.isdigit() and int(k) in quests:
                if set_string(nx.child(c, 'name'), quests[int(k)].get('name')):
                    counts['quest'] = counts.get('quest', 0) + 1

        return finish(nx, dst, new_strings, patches, counts)

    skillimg = nx.child(root, 'Skill.img')

    for k, c in (nx.children(skillimg).items() if skillimg is not None else []):
        if k.isdigit() and int(k) in skills:
            r = skills[int(k)]

            if set_string(nx.child(c, 'name'), r.get('name')):
                counts['skill'] = counts.get('skill', 0) + 1

            set_string(nx.child(c, 'desc'), r.get('desc'))

    # items: flat images and the nested equipment / etc trees
    for img in ('Consume.img', 'Ins.img', 'Cash.img', 'Pet.img'):
        base = nx.child(root, img)

        for k, c in (nx.children(base).items() if base is not None else []):
            if k.isdigit():
                item_node(c, int(k))

    etc = nx.child(nx.child(root, 'Etc.img'), 'Etc')

    for k, c in (nx.children(etc).items() if etc is not None else []):
        if k.isdigit():
            item_node(c, int(k))

    eqp = nx.child(nx.child(root, 'Eqp.img'), 'Eqp')

    for cat, cnode in (nx.children(eqp).items() if eqp is not None else []):
        for k, c in nx.children(cnode).items():
            if k.isdigit():
                item_node(c, int(k))

    # maps: Map.img/<region>/<id>/{mapName, streetName}
    mapimg = nx.child(root, 'Map.img')

    for region, rnode in (nx.children(mapimg).items() if mapimg is not None else []):
        for k, c in nx.children(rnode).items():
            if not k.isdigit():
                continue

            r = maps.get(int(k))

            if not r:
                continue

            if set_string(nx.child(c, 'mapName'), r.get('name')):
                counts['map'] = counts.get('map', 0) + 1

            set_string(nx.child(c, 'streetName'), r.get('streetName'))

    for img, table, key in (('Mob.img', mobs, 'mob'), ('Npc.img', npcs, 'npc')):
        base = nx.child(root, img)

        for k, c in (nx.children(base).items() if base is not None else []):
            if k.isdigit() and int(k) in table:
                if set_string(nx.child(c, 'name'), table[int(k)].get('name')):
                    counts[key] = counts.get(key, 0) + 1

    finish(nx, dst, new_strings, patches, counts)


def finish(nx, dst, new_strings, patches, counts):
    # write appended strings, then the new offset table
    end = os.path.getsize(dst)
    nx.m.close()
    f = nx.f
    f.seek(end)

    def align(n):
        pad = (-f.tell()) % n
        f.write(b'\0' * pad)

    align(2)
    offsets = []

    for s in new_strings:
        b = s.encode('utf8')[:65535]
        offsets.append(f.tell())
        f.write(struct.pack('<H', len(b)))
        f.write(b)
        align(2)

    align(8)
    table_off = f.tell()

    # copy the old table, then the new offsets
    m = mmap.mmap(f.fileno(), 0)
    old = m[nx.so:nx.so + nx.sc * 8]
    m.close()
    f.seek(table_off)
    f.write(old)

    for o in offsets:
        f.write(struct.pack('<Q', o))

    # header: string count and table offset
    f.seek(4 + 4 + 8)
    f.write(struct.pack('<IQ', nx.sc + len(new_strings), table_off))

    # node data: string id lives in the first 4 data bytes
    for node_idx, sid in patches:
        f.seek(nx.no + node_idx * 20 + 12)
        f.write(struct.pack('<I', sid))

    f.close()
    print('patched', counts, 'strings', len(new_strings))


if __name__ == '__main__':
    main()
