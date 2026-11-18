import struct,sys
sys.path.insert(0,'/home/claude/scripts')
from nxls import NX
def val(nx,i):
    _,_,_,t=nx.node(i); off=nx.no+i*20+12
    if t==1: return struct.unpack_from('<q',nx.m,off)[0]
    if t==2: return struct.unpack_from('<d',nx.m,off)[0]
    if t==3: return repr(nx.string(struct.unpack_from('<I',nx.m,off)[0]))
    if t==4: return 'vec%s'%(struct.unpack_from('<ii',nx.m,off),)
    if t==5: _,w,h=struct.unpack_from('<IHH',nx.m,off); return 'bmp %dx%d'%(w,h)
    if t==6: return 'audio'
    return ''
def walk(nx,i,d,maxd):
    ch=nx.children(i)
    for k in sorted(ch, key=lambda x:(len(x),x)):
        c=ch[k]; print('  '*d+k, val(nx,c))
        if d+1<maxd: walk(nx,c,d+1,maxd)
nx=NX(sys.argv[1]); i=nx.get(sys.argv[3:]); walk(nx,i,0,int(sys.argv[2]))
