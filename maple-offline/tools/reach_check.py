#!/usr/bin/env python3
"""Prototype: which footholds of a map can the player reach by normal play.

Seeds: every portal (where a player can appear). Moves: walk along a
foothold chain, jump (up to JUMP_H up, any drop), fall, ropes/ladders,
intra-map portals. Walls (vertical footholds) block sideways moves unless
they can be jumped over or passed under.
"""
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict

JUMP_H = 110      # feet rise (jump stat 123 gives ~100)
REACH_X = 260     # sideways reach in one jump (flash jump, speed 140)
STEP = 20
BODY = 50


def load(path):
    root = ET.parse(path).getroot()
    fhs, ropes, portals, info = {}, [], [], {}
    for c in root:
        n = c.get('name')
        if n == 'foothold':
            for L in c:
                for g in L:
                    for f in g:
                        d = {e.get('name'): int(e.get('value')) for e in f if e.tag == 'int'}
                        fhs[int(f.get('name'))] = dict(d, layer=int(L.get('name')), id=int(f.get('name')))
        elif n == 'ladderRope':
            for r in c:
                d = {e.get('name'): int(e.get('value')) for e in r if e.tag == 'int'}
                ropes.append(d)
        elif n == 'portal':
            for p in c:
                d = {e.get('name'): e.get('value') for e in p}
                portals.append(d)
        elif n == 'info':
            for e in c:
                info[e.get('name')] = e.get('value')
    return fhs, ropes, portals, info


class Map:
    def __init__(self, path):
        self.fhs, self.ropes, self.portals, self.info = load(path)
        self.floors = {i: f for i, f in self.fhs.items() if f['x1'] != f['x2']}
        self.walls = [f for f in self.fhs.values() if f['x1'] == f['x2']]
        self.by_x = defaultdict(list)
        for f in self.floors.values():
            for x in range(min(f['x1'], f['x2']), max(f['x1'], f['x2']) + 1):
                self.by_x[x].append(f)

    @staticmethod
    def ground(f, x):
        x1, y1, x2, y2 = f['x1'], f['y1'], f['x2'], f['y2']
        return y1 + (y2 - y1) * (x - x1) / (x2 - x1)

    def below(self, x, y):
        best, bf = None, None
        for f in self.by_x.get(int(x), ()):
            g = self.ground(f, x)
            if g >= y and (best is None or g < best):
                best, bf = g, f
        return bf, best

    def wall_blocks(self, x0, x1, ytop, ylow):
        """Sideways from x0 to x1 with feet anywhere between ytop (highest
        reachable) and ylow (lowest along the move): blocked by a wall that
        can be neither jumped over nor passed under."""
        lo, hi = min(x0, x1), max(x0, x1)
        for w in self.walls:
            wx = w['x1']
            if wx <= lo or wx >= hi:
                continue
            t, b = min(w['y1'], w['y2']), max(w['y1'], w['y2'])
            over = ytop - 1 < t          # feet above the wall top
            under = ylow - BODY > b      # head below the wall bottom
            if not over and not under:
                return True
        return False


def analyze(path):
    m = Map(path)
    # points: (x, y, group)
    pts = []
    group_of_fh = {}
    # chain groups: consecutive floors linked by prev/next
    parent = {}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        a, b = find(a), find(b)
        if a != b:
            parent[a] = b

    for i in m.floors:
        parent[('f', i)] = ('f', i)
    for i, f in m.floors.items():
        for k in ('prev', 'next'):
            j = f.get(k, 0)
            if j in m.floors:
                union(('f', i), ('f', j))
    for ri, r in enumerate(m.ropes):
        parent[('r', ri)] = ('r', ri)

    for i, f in m.floors.items():
        l, r = sorted((f['x1'], f['x2']))
        xs = list(range(l, r + 1, STEP))
        if xs[-1] != r:
            xs.append(r)
        for x in xs:
            pts.append((x, m.ground(f, x), ('f', i)))
    for ri, r in enumerate(m.ropes):
        y1, y2 = sorted((r['y1'], r['y2']))
        for y in range(y1, y2 + 1, STEP):
            pts.append((r['x'], y, ('r', ri)))

    # edges between points (union their groups when reachable both ways is
    # not needed: build a directed graph on groups)
    adj = defaultdict(set)
    grid = defaultdict(list)
    for p in pts:
        grid[(int(p[0] // REACH_X), int(p[1] // 200))].append(p)

    for p in pts:
        px, py, pg = p
        gx, gy = int(px // REACH_X), int(py // 200)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1, 2, 3):
                for q in grid.get((gx + dx, gy + dy), ()):
                    qx, qy, qg = q
                    if find(qg) == find(pg):
                        continue
                    if abs(qx - px) > REACH_X:
                        continue
                    if qy < py - JUMP_H:
                        continue
                    if qy > py + 600:
                        continue
                    # rope targets: you grab ropes when level with them
                    if m.wall_blocks(px, qx, py - JUMP_H, max(py, qy)):
                        continue
                    adj[find(pg)].add(find(qg))
        # straight drop / down-jump
        f, g = m.below(px, py + 1)
        if f is not None:
            adj[find(pg)].add(find(('f', f['id'])))

    # rope tops: the floor the rope reaches
    for ri, r in enumerate(m.ropes):
        y1 = min(r['y1'], r['y2'])
        f, g = m.below(r['x'], y1 - 15)
        if f is not None and g - y1 < 30:
            adj[find(('r', ri))].add(find(('f', f['id'])))

    # seeds: portals; intra-map portals link
    mapid = int(path.split('/')[-1].split('.')[0])
    seeds = set()
    pt_group = {}
    for p in m.portals:
        x, y = int(p['x']), int(p['y'])
        f, g = m.below(x, y - 12)
        if f is not None:
            grp = find(('f', f['id']))
            seeds.add(grp)
            pt_group[p['pn']] = grp
    for p in m.portals:
        if int(p.get('tm', 0)) == mapid and p.get('tn') in pt_group and p['pn'] in pt_group:
            adj[pt_group[p['pn']]].add(pt_group[p['tn']])

    # spring portals (pt 12/13) launch the player about 1000 px up
    for p in m.portals:
        if p.get('pt') in ('12', '13'):
            x, y = int(p['x']), int(p['y'])
            f, g = m.below(x, y - 12)
            if f is None:
                continue
            src = find(('f', f['id']))
            for (qx, qy, qg) in pts:
                if abs(qx - x) <= 250 and y - 1200 <= qy <= y:
                    adj[src].add(find(qg))

    seen = set(seeds)
    stack = list(seeds)
    while stack:
        a = stack.pop()
        for b in adj[a]:
            if b not in seen:
                seen.add(b)
                stack.append(b)

    unreach = [i for i in m.floors if find(('f', i)) not in seen]
    return m, unreach, seen, find


if __name__ == '__main__':
    tot = 0
    for path in sys.argv[1:]:
        try:
            m, un, seen, find = analyze(path)
        except Exception as e:
            print(path, 'ERR', e)
            continue
        if un:
            tot += 1
            groups = defaultdict(list)
            for i in un:
                groups[find(('f', i))].append(i)
            desc = []
            for g, ids in groups.items():
                xs = [min(m.floors[i]['x1'], m.floors[i]['x2']) for i in ids] + [max(m.floors[i]['x1'], m.floors[i]['x2']) for i in ids]
                ys = [m.floors[i]['y1'] for i in ids]
                desc.append('%d fh x %d..%d y %d..%d' % (len(ids), min(xs), max(xs), min(ys), max(ys)))
            print(path.split('/')[-1], len(m.floors), 'unreach', len(un), '|', '; '.join(desc[:4]))
    print('maps with unreachable', tot)


def explain(path):
    """For each unreachable group: the smallest rise needed from a reachable
    point within REACH_X sideways, and whether a wall was in the way."""
    m, un, seen, find = analyze(path)
    groups = defaultdict(list)
    for i in un:
        groups[find(('f', i))].append(i)
    reach_pts = []
    for i, f in m.floors.items():
        if find(('f', i)) in seen:
            l, r = sorted((f['x1'], f['x2']))
            for x in range(l, r + 1, 10):
                reach_pts.append((x, m.ground(f, x)))
    for ri, r in enumerate(m.ropes):
        if find(('r', ri)) in seen:
            for y in range(min(r['y1'], r['y2']), max(r['y1'], r['y2']) + 1, 10):
                reach_pts.append((r['x'], y))
    out = []
    for g, ids in groups.items():
        best = None
        for i in ids:
            f = m.floors[i]
            l, r = sorted((f['x1'], f['x2']))
            for x in range(l, r + 1, 10):
                y = m.ground(f, x)
                for (px, py) in reach_pts:
                    if abs(px - x) <= REACH_X and py >= y:
                        rise = py - y
                        if best is None or rise < best[0]:
                            best = (rise, px, py, x, y, m.wall_blocks(px, x, py - JUMP_H, py))
        out.append((len(ids), best))
    return out
