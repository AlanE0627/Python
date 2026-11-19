"""Move the KMS creation-board heading onto the v83 board image. Heading
boxes are measured by hand from the images (v83 text, KMS text)."""
from PIL import Image

BOXES = {
    'NewChar':       ((18, 66, 132, 85), (25, 65, 94, 85)),
    'NewCharKnight': ((14, 81, 120, 100), (24, 65, 94, 86)),
    'NewCharAran':   ((31, 63, 166, 85), (20, 71, 89, 92)),
    # name boards (only Aran keeps its v83 board; the others use KMS pictures)
    'NewCharAran/charName': ((30, 58, 156, 82), (19, 70, 90, 92)),
}

# board texture ranges to copy from (Aran: stop before the blue roller)
BG = {'NewCharAran': (170, 222), 'NewCharAran/charName': (158, 182)}

def _bg_x(x, x0, lo, hi):
    return lo + ((x - x0) % max(1, hi - lo))

def retitle(sec, v83, kms):
    if sec not in BOXES:
        return False
    (vx0, vy0, vx1, vy1), (kx0, ky0, kx1, ky1) = BOXES[sec]
    vp, kp = v83.load(), kms.load()
    vlo, vhi = BG.get(sec, (vx1 + 2, v83.width - 15))
    klo, khi = kx1 + 2, kms.width - 15
    # erase the English heading with the board texture of the same rows
    for y in range(vy0, vy1):
        for x in range(vx0, vx1):
            vp[x, y] = vp[_bg_x(x, vx0, vlo, vhi), y]
    # Korean heading: KMS pixels that stand out from the KMS board texture
    oy = vy0 + ((vy1 - vy0) - (ky1 - ky0)) // 2
    ox = vx0 + 2
    for y in range(ky0, ky1):
        for x in range(kx0, kx1):
            p = kp[x, y]
            b = kp[_bg_x(x, kx0, klo, khi), y]
            if p[3] > 0 and sum(abs(p[i] - b[i]) for i in range(3)) > 90:
                tx, ty = ox + (x - kx0), oy + (y - ky0)
                if 0 <= tx < v83.width and 0 <= ty < v83.height:
                    vp[tx, ty] = p
    return True
