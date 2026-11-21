#!/usr/bin/env python3
"""String literals of the server JS scripts (NPC / quest / portal ...).

Tokenizes each script (comments, regex-free JS as used by the Cosmic
scripts), lists every string literal with its position, and marks the ones
that are text a player reads: arguments of the dialog / message calls, and
literals that build such text through variables (var text = "..."; text +=
"#L0#...").

    js_strings.py stats   <scripts dir>
    js_strings.py dump    <script.js>            literals, one per line
    js_strings.py export  <scripts dir> <out.json>
    js_strings.py apply   <scripts dir> <translations.json> <out dir>

Translations are keyed by file + literal index + the original literal, so a
changed original is never overwritten by a stale translation.
"""
import json
import os
import re
import sys

DIALOG_CALLS = (
    'sendOk', 'sendNext', 'sendPrev', 'sendNextPrev', 'sendYesNo', 'sendSimple',
    'sendAcceptDecline', 'sendStyle', 'sendGetText', 'sendGetNumber',
    'sendDimensionalMirror', 'sendNextS', 'sendOkS', 'sendPrevS', 'sendNextPrevS',
    'sendYesNoS', 'sendSimpleS', 'sendAcceptDeclineS',
    'dropMessage', 'message', 'playerMessage', 'mapMessage', 'getClient().getChannelServer().broadcast',
    'earnTitle', 'showInfoText', 'yellowMessage', 'serverNotice', 'sendTalk',
)


def tokenize(src):
    """Yield (kind, text, start, end); kind in str, ident, punct, ws."""
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c in ' \t\r\n':
            j = i
            while j < n and src[j] in ' \t\r\n':
                j += 1
            yield 'ws', src[i:j], i, j
            i = j
        elif src.startswith('//', i):
            j = src.find('\n', i)
            j = n if j < 0 else j
            yield 'ws', src[i:j], i, j
            i = j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            yield 'ws', src[i:j], i, j
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and src[j] != c:
                if src[j] == '\\':
                    j += 1
                elif src[j] == '\n':
                    break
                j += 1
            yield 'str', src[i:j + 1], i, j + 1
            i = j + 1
        elif c.isalnum() or c in '_$':
            j = i
            while j < n and (src[j].isalnum() or src[j] in '_$'):
                j += 1
            yield 'ident', src[i:j], i, j
            i = j
        else:
            yield 'punct', c, i, i + 1
            i += 1


def unquote(lit):
    body = lit[1:-1]
    return bytes(body, 'utf-8').decode('unicode_escape', errors='replace') \
        if '\\u' in body else body.replace('\\"', '"').replace("\\'", "'")


WORDY = re.compile(r'[A-Za-z]{2,}[^A-Za-z]+[A-Za-z]{2,}')
CODEY = re.compile(r'^[\w.$/-]*$')


def literals(src):
    """All string literals with a 'dialog' flag."""
    toks = [t for t in tokenize(src) if t[0] != 'ws']
    out = []
    # variables that hold dialog text: assigned a literal that is later
    # passed to a dialog call, or appended to with +=
    dialog_vars = set()
    for k, t in enumerate(toks):
        if t[0] == 'ident' and t[1] in DIALOG_CALLS and k + 1 < len(toks) and toks[k + 1][1] == '(':
            depth = 0
            for m in range(k + 1, len(toks)):
                if toks[m][1] == '(':
                    depth += 1
                elif toks[m][1] == ')':
                    depth -= 1
                    if depth == 0:
                        break
                elif toks[m][0] == 'ident':
                    dialog_vars.add(toks[m][1])
    for k, t in enumerate(toks):
        if t[0] != 'str':
            continue
        text = unquote(t[1])
        # inside a dialog call?
        depth, call = 0, None
        for m in range(k - 1, -1, -1):
            v = toks[m][1]
            if v == ')':
                depth += 1
            elif v == '(':
                if depth == 0:
                    if m > 0 and toks[m - 1][0] == 'ident':
                        call = toks[m - 1][1]
                    break
                depth -= 1
            elif v in ';{}' and depth == 0:
                break
        assigned = None
        for m in range(k - 1, max(-1, k - 40), -1):
            v = toks[m][1]
            if v in ';{}':
                break
            if v == '=' and m > 0 and toks[m - 1][0] == 'ident':
                assigned = toks[m - 1][1]
            if v == '+' and m + 1 < len(toks) and toks[m + 1][1] == '=' and m > 0:
                assigned = toks[m - 1][1]
        dialog = (call in DIALOG_CALLS) or (assigned in dialog_vars) \
            or (call is None and assigned is None and False)
        # plain words or tags (#b #k #L0# #p..#) are readable text; ids,
        # paths and property names are not
        readable = bool(WORDY.search(text)) or ('#' in text and not CODEY.match(text))
        out.append({
            'i': len(out), 'start': t[2], 'end': t[3], 'line': src.count('\n', 0, t[2]) + 1,
            'text': text, 'call': call, 'var': assigned,
            'dialog': bool(dialog and readable) or bool(readable and len(text) > 25 and ' ' in text),
        })
    return out


def scan(root):
    for sub in sorted(os.listdir(root)):
        d = os.path.join(root, sub)
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if f.endswith('.js'):
                yield sub + '/' + f, os.path.join(d, f)


def main():
    cmd = sys.argv[1]
    if cmd == 'dump':
        src = open(sys.argv[2], encoding='utf-8').read()
        for l in literals(src):
            mark = 'D' if l['dialog'] else ' '
            print(f"{mark} {l['i']:3} L{l['line']:<4} {l['call'] or l['var'] or '':<16} {l['text'][:110]!r}")
    elif cmd == 'stats':
        tot = {}
        for rel, path in scan(sys.argv[2]):
            sub = rel.split('/')[0]
            ls = literals(open(path, encoding='utf-8').read())
            d = [l for l in ls if l['dialog']]
            hangul = sum(1 for l in d if re.search('[가-힣]', l['text']))
            a = tot.setdefault(sub, [0, 0, 0, 0])
            a[0] += 1
            a[1] += len(d)
            a[2] += sum(len(l['text']) for l in d)
            a[3] += hangul
        for sub, (files, n, chars, hangul) in tot.items():
            print(f'{sub:10} files {files:4}  dialog literals {n:6}  chars {chars:8}  already Korean {hangul}')
    elif cmd == 'export':
        data = {}
        for rel, path in scan(sys.argv[2]):
            ls = literals(open(path, encoding='utf-8').read())
            d = [{'i': l['i'], 'line': l['line'], 'text': l['text']} for l in ls if l['dialog']]
            if d:
                data[rel] = d
        json.dump(data, open(sys.argv[3], 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    elif cmd == 'apply':
        root, trans, out = sys.argv[2], json.load(open(sys.argv[3], encoding='utf-8')), sys.argv[4]
        done = skipped = 0
        for rel, items in trans.items():
            path = os.path.join(root, rel)
            src = open(path, encoding='utf-8').read()
            ls = literals(src)
            edits = []
            for it in items:
                l = ls[it['i']] if it['i'] < len(ls) else None
                if l is None or l['text'] != it['text'] or not it.get('ko'):
                    skipped += 1
                    continue
                q = src[l['start']]
                ko = it['ko'].replace('\\', '\\\\').replace(q, '\\' + q).replace('\n', '\\n').replace('\r', '\\r')
                edits.append((l['start'], l['end'], q + ko + q))
                done += 1
            for s, e, rep in sorted(edits, reverse=True):
                src = src[:s] + rep + src[e:]
            dst = os.path.join(out, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, 'w', encoding='utf-8').write(src)
        print('applied', done, 'skipped', skipped)


if __name__ == '__main__':
    main()
