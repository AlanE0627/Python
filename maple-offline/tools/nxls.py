import struct,sys,mmap
class NX:
    def __init__(s,p):
        f=open(p,'rb'); s.m=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
        assert s.m[:4]==b'PKG4'
        s.nc,s.no,s.sc,s.so=struct.unpack_from('<IQIQ',s.m,4)
    def node(s,i):
        name,first,cnt,typ=struct.unpack_from('<IIHH',s.m,s.no+i*20); return name,first,cnt,typ
    def string(s,i):
        off=struct.unpack_from('<Q',s.m,s.so+i*8)[0]; l=struct.unpack_from('<H',s.m,off)[0]
        return s.m[off+2:off+2+l].decode('utf8','replace')
    def children(s,i):
        _,first,cnt,_=s.node(i); return {s.string(s.node(c)[0]):c for c in range(first,first+cnt)}
    def get(s,path):
        i=0
        for k in path:
            ch=s.children(i)
            if k not in ch: return None
            i=ch[k]
        return i
if __name__=='__main__':
    nx=NX(sys.argv[1]); path=sys.argv[2:]
    i=nx.get(path); print(sorted(nx.children(i).keys()) if i is not None else 'MISSING')
