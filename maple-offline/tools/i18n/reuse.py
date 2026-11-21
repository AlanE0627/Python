# fill translations for texts already translated in earlier batches (same English text -> same Korean)
import json, os, sys
D = '/home/claude/i18n/scripts'
memo = {}
# job-specific Korean must not leak into another class's script
JOBWORDS = ('궁수', '헌터', '사수', '마법사', '위자드', '클레릭', '전사', '파이터', '페이지', '스피어맨', '도적', '어쌔신', '시프', '해적', '인파이터', '건슬링거')
for p in sorted(os.listdir(D)):
    if p.endswith('.json'):
        for f, rows in json.load(open(f'{D}/{p}')).items():
            for r in rows:
                if r.get('ko') and len(r['text']) > 3:
                    memo.setdefault(r['text'], r['ko'])
src = json.load(open(sys.argv[1]))
hit = {}
miss = 0
for f, items in src.items():
    for it in items:
        if it['text'] in memo and not any(w in memo[it['text']] for w in JOBWORDS):
            hit.setdefault(f, {})[it['i']] = memo[it['text']]
        else:
            miss += 1
json.dump(hit, open(sys.argv[2], 'w'), ensure_ascii=False, indent=1)
print('reused', sum(len(v) for v in hit.values()), 'remaining', miss)
