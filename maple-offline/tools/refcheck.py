import re,sys,glob,collections
sys.path.insert(0,'/home/claude/scripts')
from nxls import NX
src=sys.argv[1]; nxdir=sys.argv[2]
pat=re.compile(r'nl::nx::([a-z0-9_]+)((?:\[\s*"[^"]+"\s*\])+)')
refs=collections.defaultdict(set)
for f in glob.glob(src+'/**/*.cpp',recursive=True)+glob.glob(src+'/**/*.h',recursive=True):
    for m in pat.finditer(open(f,errors='ignore').read()):
        refs[m.group(1)].add(tuple(re.findall(r'"([^"]+)"',m.group(2))))
names={'ui':'UI','map':'Map','character':'Character','effect':'Effect','etc':'Etc','item':'Item','mob':'Mob','npc':'Npc','quest':'Quest','reactor':'Reactor','skill':'Skill','sound':'Sound','string':'String','base':'Base','morph':'Morph','tamingmob':'TamingMob'}
miss_all={}
for k,paths in sorted(refs.items()):
    if k not in names: print(f'{k}: {len(paths)} refs -> no v83 file (post-v83 asset)'); continue
    nx=NX(f'{nxdir}/{names[k]}.nx'); miss=[p for p in paths if nx.get(p) is None]
    print(f'{k}: {len(paths)} literal paths, missing in v83 {len(miss)}')
    miss_all[k]=miss
for k,m in miss_all.items():
    tops=collections.Counter(p[0] for p in m)
    if m: print(k,'missing by top node:',dict(tops.most_common(12)))
