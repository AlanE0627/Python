#!/usr/bin/env python3
"""maple-offline: build UI_ko.nx = base UI.nx with chosen top-level imgs
replaced by imgs read from a KMS client's Data/UI folder.

usage: nx_merge_ui.py <base UI.nx | --overlay> <out UI_ko.nx> <Data/UI dir> <Img.img | Img.img:Section | Back/x.img=path> [...]
Output is for the user's own device; never commit it."""
import os, sys, struct, mmap
import lz4.block
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wzimg

KEY = wzimg.keystream(wzimg.IVS['KMS'], 0x100000)
WZ = None  # packed UI.wz source (KMS 1.2.95) when the "dir" argument ends with .wz

def read_img(uidir, img):
    """(data, tree) for a top-level img, from a Data/UI folder or a packed UI.wz."""
    if WZ is not None:
        return WZ.d, WZ.img(img)
    data = open(os.path.join(uidir, img), 'rb').read()
    return data, wzimg.parse_img(data, KEY)

class Base:
    def __init__(self, path):
        self.f = open(path, 'rb'); self.m = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
        (self.nc, self.no, self.sc, self.so, self.bc, self.bo, self.ac, self.ao) = struct.unpack_from('<IQIQIQIQ', self.m, 4)
    def node(self, i):
        return struct.unpack_from('<IIHH8s', self.m, self.no + i * 20)
    def string(self, i):
        off = struct.unpack_from('<Q', self.m, self.so + i * 8)[0]
        n = struct.unpack_from('<H', self.m, off)[0]
        return self.m[off + 2:off + 2 + n].decode('utf8', 'replace')
    def bitmap_blob(self, i):
        off = struct.unpack_from('<Q', self.m, self.bo + i * 8)[0]
        n = struct.unpack_from('<I', self.m, off)[0]
        return self.m[off:off + 4 + n]
    def audio_blob(self, i, ln):
        off = struct.unpack_from('<Q', self.m, self.ao + i * 8)[0]
        return self.m[off:off + ln]

# output node: [name, children(list), type, payload]
def from_base(b, i):
    name, first, cnt, typ, data = b.node(i)
    n = [b.string(name), None, typ, None]
    if typ == 1 or typ == 2 or typ == 4:
        n[3] = data
    elif typ == 3:
        n[3] = ('s', b.string(struct.unpack_from('<I', data)[0]))
    elif typ == 5:
        bid, w, h = struct.unpack_from('<IHH', data); n[3] = ('bb', b.bitmap_blob(bid), w, h)
    elif typ == 6:
        aid, ln = struct.unpack_from('<II', data); n[3] = ('ab', b.audio_blob(aid, ln), ln)
    n[1] = [from_base(b, c) for c in range(first, first + cnt)]
    return n

def from_aria(data, name, node, root, path):
    t = node.type
    n = [name, [], 0, None]
    if t == 'int':
        n[2] = 1; n[3] = struct.pack('<q', node.value)
    elif t == 'float':
        n[2] = 2; n[3] = struct.pack('<d', node.value)
    elif t == 'string':
        n[2] = 3; n[3] = ('s', node.value)
    elif t == 'vector':
        n[2] = 4; n[3] = struct.pack('<ii', *node.value)
    elif t == 'canvas':
        w, h, px = wzimg.canvas_pixels(data, node, KEY)
        comp = lz4.block.compress(px, store_size=False)
        n[2] = 5; n[3] = ('bb', struct.pack('<I', len(comp)) + comp, w, h)
    elif t == 'uol':
        tgt = resolve_uol(root, path, node.value)
        if tgt is not None:
            m = from_aria(data, name, tgt[0], root, tgt[1]); return m
        n[2] = 0
    for k, v in node.items():
        n[1].append(from_aria(data, k, v, root, path + [k]))
    return n

def deep_merge(new, old):
    """add children that exist only in old (recursively for dirs)."""
    have = {c[0]: c for c in new[1]}
    for c in old[1]:
        if c[0] not in have:
            new[1].append(c)
        elif have[c[0]][2] == 0 and c[2] == 0:
            deep_merge(have[c[0]], c)

def resolve_uol(root, path, target, depth=0):
    if depth > 8:
        return None
    parts = path[:-1]
    for seg in target.split('/'):
        if seg == '..':
            parts = parts[:-1]
        elif seg:
            parts = parts + [seg]
    cur = root
    for s in parts:
        if s not in cur:
            return None
        cur = cur[s]
    if cur.type == 'uol':
        return resolve_uol(root, parts, cur.value, depth + 1)
    return cur, parts

def write(out, root):
    nodes = []; strings = {}; slist = []; bitmaps = []; audios = []
    def sid(s):
        if s not in strings:
            strings[s] = len(slist); slist.append(s)
        return strings[s]
    # BFS so that children are contiguous
    order = [root]; idx = 0
    firsts = {}
    while idx < len(order):
        n = order[idx]
        kids = sorted(n[1], key=lambda c: c[0])
        n[1] = kids
        firsts[id(n)] = len(order)
        order.extend(kids)
        idx += 1
    recs = bytearray()
    for n in order:
        name, kids, typ, pay = n
        data = b'\0' * 8
        if typ in (1, 2, 4):
            data = pay
        elif typ == 3:
            data = struct.pack('<I', sid(pay[1])) + b'\0' * 4
        elif typ == 5:
            data = struct.pack('<IHH', len(bitmaps), pay[2], pay[3]); bitmaps.append(pay[1])
        elif typ == 6:
            data = struct.pack('<II', len(audios), pay[2]); audios.append(pay[1])
        recs += struct.pack('<IIHH', sid(name), firsts[id(n)] if kids else 0, len(kids), typ) + data
    with open(out, 'wb') as f:
        f.write(b'\0' * 52)
        def align(a):
            f.write(b'\0' * ((-f.tell()) % a))
        align(16); no = f.tell(); f.write(recs)
        soffs = []
        for s in slist:
            align(2); soffs.append(f.tell()); b = s.encode('utf8')[:65535]
            f.write(struct.pack('<H', len(b)) + b)
        align(8); so = f.tell()
        for o in soffs: f.write(struct.pack('<Q', o))
        boffs = []
        for blob in bitmaps:
            align(8); boffs.append(f.tell()); f.write(blob)
        align(8); bo = f.tell()
        for o in boffs: f.write(struct.pack('<Q', o))
        aoffs = []
        for blob in audios:
            align(8); aoffs.append(f.tell()); f.write(blob)
        align(8); ao = f.tell()
        for o in aoffs: f.write(struct.pack('<Q', o))
        f.seek(0)
        f.write(b'PKG4' + struct.pack('<IQIQIQIQ', len(order), no, len(slist), so, len(bitmaps), bo if bitmaps else 0, len(audios), ao if audios else 0))
    print('nodes', len(order), 'strings', len(slist), 'bitmaps', len(bitmaps), 'audio', len(audios))

def main():
    global WZ, KEY
    base, out, uidir = sys.argv[1:4]
    if uidir.endswith('.wz'):
        KEY = wzimg.keystream(wzimg.IVS['KMS_OLD'], 0x100000)
        WZ = wzimg.WzFile(uidir, KEY)
    subs = [a for a in sys.argv[4:] if ':' in a]     # Login.img:RaceSelect (one section only)
    dels = [a[1:] for a in sys.argv[4:] if a.startswith('-')]
    sys.argv[4:] = [a for a in sys.argv[4:] if not a.startswith('-')]
    # Img.img!A,B : take the KMS img but keep the base (v83) sections A, B
    drops = {}
    args = []
    for a in sys.argv[4:]:
        if '!' in a and '=' not in a:
            img, secs = a.split('!', 1)
            drops[img] = set(secs.split(','))
            a = img
        args.append(a)
    sys.argv[4:] = args
    # Img.img~ : v83 img with every KMS canvas of the same path and size swapped in
    swaps = [a[:-1] for a in sys.argv[4:] if a.endswith('~')]
    sys.argv[4:] = [a for a in sys.argv[4:] if not a.endswith('~')]
    imgs = [a for a in sys.argv[4:] if '=' not in a and ':' not in a]
    extras = [a for a in sys.argv[4:] if '=' in a]   # Back/login.img=<path>
    if base.startswith('--overlay'):
        root = ['', [], 0, None]          # only the parts given below
        b = Base(base.split('=', 1)[1]) if '=' in base else None
    else:
        b = Base(base)
        root = from_base(b, 0)
    if extras:
        # MapRes.img/<Back|Obj>/<name>.img: map back/obj overrides read by the client
        res = ['MapRes.img', [], 0, None]
        groups = {}
        for e in extras:
            rel, path = e.split('=', 1)
            grp, name = rel.split('/', 1)
            if path.endswith('.wz'):
                wz = wzimg.WzFile(path, wzimg.keystream(wzimg.IVS['KMS_OLD'], 0x100000))
                data, tree = wz.d, wz.img(rel)
            else:
                data = open(path, 'rb').read()
                tree = wzimg.parse_img(data, KEY)
            groups.setdefault(grp, ['%s' % grp, [], 0, None])[1].append(from_aria(data, name, tree, tree, []))
            print('extra', rel)
        res[1] = list(groups.values())
        root[1] = [c for c in root[1] if c[0] != 'MapRes.img'] + [res]
    for img in swaps:
        data, tree = read_img(uidir, img)
        base_img = None
        for i0 in range(b.node(0)[2]):
            ci = b.node(0)[1] + i0
            if b.string(b.node(ci)[0]) == img:
                base_img = from_base(b, ci)
        if base_img is None:
            print('no base for', img); continue
        count = [0]
        # images the client places by centre or by their own origin: any size
        # works as long as the origin comes along with the picture
        FREE = ('Quest/Tab/', 'Quest/reward', 'UserList/Tab/')
        def free(path):
            p = '/'.join(path)
            return any(p.startswith(f) for f in FREE)
        def size_ok(path, old_wh, new_wh):
            if old_wh == new_wh or free(path):
                return True
            # tab and button labels: small images placed by their top-left
            # corner; a few pixels of difference does not move anything
            label = any(seg.startswith(('Tab', 'Bt')) for seg in path)
            return (label and abs(old_wh[0] - new_wh[0]) <= 16
                    and abs(old_wh[1] - new_wh[1]) <= 6)

        rejects = []
        def swap(vn, kn, path=()):
            kids = {c[0]: c for c in vn[1]}
            for k, v in kn.items():
                c = kids.get(k)
                if c is None:
                    continue
                if v.type == 'canvas' and c[2] == 5 and not size_ok(path + (k,), (c[3][2], c[3][3]), tuple(v.value[:2])):
                    rejects.append(('/'.join(path + (k,)), (c[3][2], c[3][3]), tuple(v.value[:2])))
                if v.type == 'canvas' and c[2] == 5 and size_ok(path + (k,), (c[3][2], c[3][3]), tuple(v.value[:2])):
                    try:
                        w, h, px = wzimg.canvas_pixels(data, v, KEY)
                    except Exception:
                        continue
                    comp = lz4.block.compress(px, store_size=False)
                    c[3] = ('bb', struct.pack('<I', len(comp)) + comp, w, h)
                    count[0] += 1
                    if free(path + (k,)) and 'origin' in v:
                        for oc in c[1]:
                            if oc[0] == 'origin' and oc[2] == 4:
                                oc[3] = struct.pack('<ii', *v['origin'].value)
                if len(v):
                    swap(c, v, path + (k,))
        swap(base_img, tree)
        root[1] = [c for c in root[1] if c[0] != img] + [base_img]
        print('swapped', img, count[0], 'canvases')
        if os.environ.get('SWAP_REJECTS'):
            for r in rejects: print('  reject', img, *r)
    for sub in subs:
        img, sect = sub.split(':', 1)
        data, tree = read_img(uidir, img)
        if sect not in tree:
            print('missing', sub); continue
        new = from_aria(data, sect, tree[sect], tree, [sect])
        holder = next((c for c in root[1] if c[0] == img), None)
        if holder is None:
            holder = [img, [], 0, None]; root[1].append(holder)
        holder[1] = [c for c in holder[1] if c[0] != sect] + [new]
        print('replaced section', sub)
    for img in imgs:
        data, tree = read_img(uidir, img)
        new = from_aria(data, img, tree, tree, [])
        new[1] = [c for c in new[1] if c[0] not in drops.get(img, ())]
        old = next((c for c in root[1] if c[0] == img), None)
        if old is None and b is not None:
            # overlay: still keep v83-only sections of this img (deep merge)
            for i0 in range(b.node(0)[2]):
                ci = b.node(0)[1] + i0
                if b.string(b.node(ci)[0]) == img:
                    old = from_base(b, ci)
        if old is not None:
            deep_merge(new, old)
            old = None
        if old is not None:
            have = {c[0] for c in new[1]}
            # keep base-only sections (e.g. v83 buttons the KMS img lacks)
            new[1] += [c for c in old[1] if c[0] not in have]
        root[1] = [c for c in root[1] if c[0] != img] + [new]
        print('replaced', img)
    # -Img.img/a/b : remove a node from the result (e.g. an unwanted login map object)
    for d in dels:
        parts = d.split('/')
        cur = root
        for seg in parts[:-1]:
            cur = next((c for c in cur[1] if c[0] == seg), None)
            if cur is None:
                break
        if cur is not None:
            cur[1] = [c for c in cur[1] if c[0] != parts[-1]]
            print('removed', d)
    write(out, root)

if __name__ == '__main__':
    main()
