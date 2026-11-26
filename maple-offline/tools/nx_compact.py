#!/usr/bin/env python3
"""maple-offline: compact an NX file whose string table was patched many times.

nx_ko_patch.py appends new strings and a new string offset table on every
run; old strings and tables stay in the file. This rewrites the file with
only the strings the nodes still reference (node names and string nodes),
deduplicated, keeping node order, so the content is the same and the size
shrinks. Files with bitmaps or audio are refused (only String/Quest NX).

usage: nx_compact.py <in.nx> <out.nx>
"""
import struct
import sys


def main(src, dst):
    m = open(src, 'rb').read()
    assert m[:4] == b'PKG4'
    nc, no, sc, so, bc, bo, ac, ao = struct.unpack_from('<IQIQIQIQ', m, 4)
    if bc or ac:
        sys.exit('bitmaps/audio present, not supported')

    def string(i):
        off = struct.unpack_from('<Q', m, so + i * 8)[0]
        n = struct.unpack_from('<H', m, off)[0]
        return m[off + 2:off + 2 + n]

    nodes = bytearray(m[no:no + nc * 20])
    new_ids = {}
    strings = []

    def remap(old):
        s = string(old)
        i = new_ids.get(s)
        if i is None:
            i = len(strings)
            new_ids[s] = i
            strings.append(s)
        return i

    for i in range(nc):
        base = i * 20
        name = struct.unpack_from('<I', nodes, base)[0]
        struct.pack_into('<I', nodes, base, remap(name))
        typ = struct.unpack_from('<H', nodes, base + 10)[0]
        if typ == 3:
            sid = struct.unpack_from('<I', nodes, base + 12)[0]
            struct.pack_into('<I', nodes, base + 12, remap(sid))

    out = bytearray(52)
    def align(n):
        while len(out) % n:
            out.append(0)
    align(4)
    node_off = len(out)
    out += nodes
    align(2)
    str_offsets = []
    for s in strings:
        align(2)
        str_offsets.append(len(out))
        out += struct.pack('<H', len(s)) + s
    align(8)
    table_off = len(out)
    for o in str_offsets:
        out += struct.pack('<Q', o)
    struct.pack_into('<4sIQIQIQIQ', out, 0, b'PKG4', nc, node_off, len(strings), table_off, 0, 0, 0, 0)
    open(dst, 'wb').write(out)
    print('nodes', nc, 'strings', sc, '->', len(strings), 'size', len(m), '->', len(out))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
