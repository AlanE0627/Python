import sys, re, json, struct
sys.path.insert(0, '/home/claude/tools')
from nx_merge_ui import Base
_B = Base('/home/claude/repos/maple-offline-app/app/src/main/assets/ko/String_ko.nx')

def _children(i):
    name, first, cnt, typ, data = _B.node(i)
    return {_B.string(_B.node(first + k)[0]): first + k for k in range(cnt)}

_PD = _children(_children(0)['PetDialog.img'])

class _Str:
    def __init__(self, v): self.value = v

class _Pet:
    def __init__(self, idx): self.c = _children(idx)
    def get(self, k):
        i = self.c.get(k)
        if i is None:
            return None
        name, first, cnt, typ, data = _B.node(i)
        return _Str(_B.string(struct.unpack('<I', data[:4])[0])) if typ == 3 else None

class _KD:
    # the client matches pet commands against String_ko.nx, so read the words from there
    def get(self, pid):
        return _Pet(_PD[pid]) if pid in _PD else None

KD = _KD()
WZ = '/home/claude/repos/maple-offline-app/app/src/main/assets/wz/Item.wz/Pet/'

def table(pid):
    s = open(f'{WZ}{pid}.img.xml', encoding='utf-8').read()
    s = s[s.find('name="interact"'):]
    rows = {}
    for m in re.finditer(r'<string name="command" value="(c\d+)"/>.*?<int name="l0" value="(\d+)"/>\s*<int name="l1" value="(\d+)"/>', s, re.S):
        c, l0, l1 = m.group(1), int(m.group(2)), int(m.group(3))
        a = rows.setdefault(c, [l0, l1])
        a[0] = min(a[0], l0); a[1] = max(a[1], l1)
    node = KD.get(str(pid))
    merged = {}
    order = []
    for c, (l0, l1) in sorted(rows.items(), key=lambda x: (x[1][0], int(x[0][1:]))):
        words = node.get(c).value if node is not None and node.get(c) is not None else None
        if not words:
            continue
        key = ', '.join(w for w in words.split('|') if w)
        if key not in merged:
            merged[key] = [l0, l1]
            order.append(key)
        else:
            merged[key][0] = min(merged[key][0], l0)
            merged[key][1] = max(merged[key][1], l1)
    return '\\r\\n'.join(f"#b{k}#k (레벨 {merged[k][0]} ~ {merged[k][1]})" for k in order)

if __name__ == '__main__':
    print(table(int(sys.argv[1])))
