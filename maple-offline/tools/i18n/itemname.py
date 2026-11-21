# English (GMS v83) item name -> Korean (KMS) item name, via item id
import json, re
_g = {}
for x in json.load(open('/home/claude/i18n/GMS_83_item.json')):
    if x.get('name'):
        _g.setdefault(x['name'].strip().lower(), x['id'])
_k = {x['id']: x.get('name') for x in json.load(open('/home/claude/i18n/KMS_284_item.json'))}

def ko_item(en):
    i = _g.get(en.strip().lower())
    return _k.get(i) if i is not None else None

JOB = {'Magician': '마법사', 'Warrior': '전사', 'Bowman': '궁수', 'Thief': '도적', 'Pirate': '해적', 'Common': '공용'}

def ko_list_entry(s):
    # "Lemona#k - Magician Lv. 15#b" style menu entries
    m = re.match(r'(.+?)#k - (\w+) Lv\. (\d+)(.*)$', s)
    if not m:
        return None
    name = ko_item(m.group(1)) or m.group(1)
    return f"{name}#k - {JOB.get(m.group(2), m.group(2))} Lv. {m.group(3)}{m.group(4)}"
