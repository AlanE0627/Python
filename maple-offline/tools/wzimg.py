"""Minimal WZ .img reader (standalone img files, as in the 'Data/' client layout).
Supports the property types needed for strings/quests/UI (canvas bitmaps decoded
to raw BGRA4444/8888 bytes on request)."""
import struct, zlib
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

USERKEY = bytes([0x13,0,0,0,0x08,0,0,0,0x06,0,0,0,0xB4,0,0,0,0x1B,0,0,0,0x0F,0,0,0,0x33,0,0,0,0x52,0,0,0])
IVS = {'GMS': bytes.fromhex('4D23C72B'), 'KMS': bytes.fromhex('B97D63E9'), 'NONE': bytes(4)}

def keystream(iv, n):
    if iv == bytes(4):
        return bytes(n)
    enc = Cipher(algorithms.AES(USERKEY), modes.ECB()).encryptor()
    out = bytearray(); block = iv * 4
    while len(out) < n:
        block = enc.update(block); out += block
    return bytes(out[:n])

class Reader:
    def __init__(self, data, key):
        self.d = data; self.p = 0; self.key = key
        self.strcache = {}
    def u8(self): v = self.d[self.p]; self.p += 1; return v
    def s8(self): v = struct.unpack_from('<b', self.d, self.p)[0]; self.p += 1; return v
    def u16(self): v = struct.unpack_from('<H', self.d, self.p)[0]; self.p += 2; return v
    def s16(self): v = struct.unpack_from('<h', self.d, self.p)[0]; self.p += 2; return v
    def s32(self): v = struct.unpack_from('<i', self.d, self.p)[0]; self.p += 4; return v
    def u32(self): v = struct.unpack_from('<I', self.d, self.p)[0]; self.p += 4; return v
    def f32(self): v = struct.unpack_from('<f', self.d, self.p)[0]; self.p += 4; return v
    def f64(self): v = struct.unpack_from('<d', self.d, self.p)[0]; self.p += 8; return v
    def cint(self):
        b = self.s8()
        return self.s32() if b == -128 else b
    def clong(self):
        b = self.s8()
        if b == -128:
            v = struct.unpack_from('<q', self.d, self.p)[0]; self.p += 8; return v
        return b
    def cfloat(self):
        b = self.s8()
        return self.f32() if b == -128 else 0.0
    def raw_string(self):
        start = self.p
        n = self.s8()
        if n == 0:
            return ''
        if n > 0:  # unicode
            if n == 127: n = self.s32()
            raw = self.d[self.p:self.p + n * 2]; self.p += n * 2
            k = self.key; out = []
            mask = 0xAAAA
            for i in range(n):
                c = raw[i*2] | (raw[i*2+1] << 8)
                c ^= mask; c ^= k[i*2] | (k[i*2+1] << 8)
                mask = (mask + 1) & 0xFFFF
                out.append(chr(c))
            return ''.join(out)
        else:      # ascii / single byte (KMS clients store CP949 here)
            n = -n
            if n == 128: n = self.s32()
            raw = self.d[self.p:self.p + n]; self.p += n
            k = self.key; mask = 0xAA; b = bytearray(n)
            for i in range(n):
                b[i] = raw[i] ^ mask ^ k[i]; mask = (mask + 1) & 0xFF
            try:
                return b.decode('ascii')
            except UnicodeDecodeError:
                return b.decode('cp949', errors='replace')
    def string_block(self, base):
        t = self.u8()
        if t in (0x00, 0x73):
            return self.raw_string()
        if t in (0x01, 0x1B):
            off = self.s32(); save = self.p
            self.p = base + off; s = self.raw_string(); self.p = save
            return s
        raise ValueError('bad string block %x at %d' % (t, self.p))

def ensure_key(maxlen=0x10000):
    pass

class Node(dict):
    """children in insertion order; .value for leaf, .type tag"""
    __slots__ = ('type', 'value')
    def __init__(self, typ='dir', value=None):
        super().__init__(); self.type = typ; self.value = value

def parse_img(data, key, base=0):
    r = Reader(data, key)
    root = Node('dir')
    r.p = base
    t = r.string_block(base)
    _parse_extended(r, t, root, base)
    return root

def _parse_list(r, node, base):
    r.p += 2  # unknown short
    count = r.cint()
    for _ in range(count):
        name = r.string_block(base)
        t = r.u8()
        if t == 0:
            node[name] = Node('null')
        elif t in (2, 11):
            node[name] = Node('int', r.s16())
        elif t in (3, 19):
            node[name] = Node('int', r.cint())
        elif t == 20:
            node[name] = Node('int', r.clong())
        elif t == 4:
            node[name] = Node('float', r.cfloat())
        elif t == 5:
            node[name] = Node('float', r.f64())
        elif t == 8:
            node[name] = Node('string', r.string_block(base))
        elif t == 9:
            size = r.u32(); end = r.p + size
            child = Node('dir')
            et = r.string_block(base)
            _parse_extended(r, et, child, base)
            node[name] = child
            r.p = end
        else:
            raise ValueError('bad prop type %d at %d' % (t, r.p))

def _parse_extended(r, t, node, base):
    if t == 'Property':
        node.type = 'dir'; _parse_list(r, node, base)
    elif t == 'Canvas':
        r.u8()
        has = r.u8()
        if has == 1:
            _parse_list(r, node, base)
        w = r.cint(); h = r.cint(); fmt = r.cint(); fmt2 = r.u8()
        r.p += 4
        ln = r.s32()
        r.p += 1
        node.type = 'canvas'
        node.value = (w, h, fmt + fmt2, r.p, ln - 1)  # raw compressed data slice
        r.p += ln - 1
    elif t == 'Shape2D#Vector2D':
        node.type = 'vector'; node.value = (r.cint(), r.cint())
    elif t == 'Shape2D#Convex2D':
        node.type = 'convex'
        n = r.cint()
        for i in range(n):
            ct = r.string_block(0 if False else base)
            ch = Node('dir'); _parse_extended(r, ct, ch, base); node[str(i)] = ch
    elif t == 'Sound_DX8':
        node.type = 'sound'; node.value = None
        raise ValueError('sound not supported')
    elif t == 'UOL':
        r.u8(); node.type = 'uol'; node.value = r.string_block(base)
    else:
        raise ValueError('unknown extended type %r' % t)

def detect_key(data):
    for name, iv in IVS.items():
        k = keystream(iv, 64)
        r = Reader(data, k)
        try:
            if r.u8() == 0x73 and r.raw_string() == 'Property':
                return name
        except Exception:
            pass
    return None

def canvas_pixels(data, node, key):
    """Return (w, h, BGRA8888 bytes) for a canvas node."""
    w, h, fmt, off, ln = node.value
    raw = data[off:off + ln]
    if raw[:2] in (b'\x78\x9c', b'\x78\xda', b'\x78\x01', b'\x78\x5e'):
        z = raw
    else:
        out = bytearray(); p = 0
        while p + 4 <= len(raw):
            bl = struct.unpack_from('<i', raw, p)[0]; p += 4
            if bl <= 0 or p + bl > len(raw):
                break
            blk = raw[p:p + bl]; p += bl
            out += bytes(a ^ b for a, b in zip(blk, key[:bl]))
        z = bytes(out)
    pix = zlib.decompressobj().decompress(z)
    if fmt == 2:
        return w, h, pix[:w * h * 4]
    if fmt == 1:  # BGRA4444 -> BGRA8888
        o = bytearray(w * h * 4)
        for i in range(w * h):
            lo = pix[2 * i]; hi = pix[2 * i + 1]
            b = lo & 15; g = lo >> 4; r = hi & 15; a = hi >> 4
            o[4*i] = b * 17; o[4*i+1] = g * 17; o[4*i+2] = r * 17; o[4*i+3] = a * 17
        return w, h, bytes(o)
    if fmt == 513:  # RGB565
        o = bytearray(w * h * 4)
        for i in range(w * h):
            v = pix[2*i] | (pix[2*i+1] << 8)
            r = (v >> 11) & 31; g = (v >> 5) & 63; b = v & 31
            o[4*i] = b * 255 // 31; o[4*i+1] = g * 255 // 63; o[4*i+2] = r * 255 // 31; o[4*i+3] = 255
        return w, h, bytes(o)
    raise ValueError('canvas format %d not supported' % fmt)
