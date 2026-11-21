# collect untranslated npc/quest scripts for NPCs placed on maps [lo, hi) -> <out dir>, export json
import os, re, glob, json, shutil, sys, subprocess
lo, hi, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
A = '/home/claude/repos/maple-offline-app/app/src/main/assets'
O = '/home/claude/i18n/orig_scripts'
names = {x['id']: x.get('name') for x in json.load(open('/home/claude/i18n/KMS_284_npc.json'))}
npcs = {}
for f in glob.glob(f'{A}/wz/Map.wz/Map/Map*/*.img.xml'):
    mid = int(os.path.basename(f).split('.')[0])
    if not (lo <= mid < hi):
        continue
    s = open(f, encoding='utf-8').read()
    for m in re.finditer(r'<string name="type" value="n"/>.*?<string name="id" value="(\d+)"/>', s, re.S):
        npcs.setdefault(int(m.group(1)), set()).add(mid)
done = set()
for p in os.listdir('/home/claude/i18n/scripts'):
    if p.endswith('.json'):
        done |= set(json.load(open('/home/claude/i18n/scripts/' + p)).keys())
shutil.rmtree(out, ignore_errors=True)
os.makedirs(out + '/npc'); os.makedirs(out + '/quest')
sel = []
for n in sorted(npcs):
    if os.path.exists(f'{O}/npc/{n}.js') and f'npc/{n}.js' not in done:
        shutil.copy(f'{O}/npc/{n}.js', out + '/npc/'); sel.append((n, names.get(n)))
print('npcs', sel)
s = open(f'{A}/wz/Quest.wz/Check.img.xml', encoding='utf-8').read()
qs = set()
for m in re.finditer(r'<imgdir name="(\d+)">\s*<imgdir name="0">(.*?)</imgdir>\s*<imgdir name="1">(.*?)</imgdir>', s, re.S):
    for g in (2, 3):
        n = re.search(r'name="npc" value="(\d+)"', m.group(g))
        if n and int(n.group(1)) in npcs:
            qs.add(int(m.group(1)))
have = [q for q in sorted(qs) if os.path.exists(f'{O}/quest/{q}.js') and f'quest/{q}.js' not in done]
print('quests', have)
for q in have:
    shutil.copy(f'{O}/quest/{q}.js', out + '/quest/')
subprocess.run(['python3', '/home/claude/tools/i18n/js_strings.py', 'export', out, out + '.json'])
d = json.load(open(out + '.json'))
for f, v in d.items():
    print(f, len(v), sum(len(i['text']) for i in v))
print('TOTAL', sum(len(v) for v in d.values()), sum(len(i['text']) for v in d.values() for i in v))
