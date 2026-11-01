# Copyright 2026 ALTESSA SOLUTIONS INC.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.
"""Synthetic ZIP fixtures. No tablet files or third-party document payloads."""
import struct as s, zlib, random

def zip_bytes(files, descriptor=0, local_extra=b'', central_extra=b'', comment=b''):
    body=bytearray(); directory=bytearray()
    for name,data,method in files:
        name=name.encode() if isinstance(name,str) else name
        crc=zlib.crc32(data); payload=data
        if method==8:
            c=zlib.compressobj(wbits=-15); payload=c.compress(data)+c.flush()
        offset=len(body); flags=8 if descriptor else 0
        body+=s.pack('<IHHHHHIIIHH',0x04034b50,20,flags,method,0,33,
                     0 if descriptor else crc,0 if descriptor else len(payload),0 if descriptor else len(data),len(name),len(local_extra))
        body+=name+local_extra+payload
        if descriptor:
            if descriptor==2: body+=s.pack('<I',0x08074b50)
            body+=s.pack('<III',crc,len(payload),len(data))
        directory+=s.pack('<IHHHHHHIIIHHHHHII',0x02014b50,20,20,flags,method,0,33,crc,len(payload),len(data),len(name),len(central_extra),0,0,0,0,offset)
        directory+=name+central_extra
    return bytes(body+directory+s.pack('<IHHHHIIH',0x06054b50,0,0,len(files),len(files),len(directory),len(body),len(comment))+comment)

def cases():
    files=[('doc.content',b'{"pages":[]}',0),('doc/page.rm',bytes(range(128))*10,8),('other.metadata',b'',0)]
    seed=zip_bytes(files)
    yield {'data':seed}
    for d in (1,2): yield {'data':zip_bytes(files,descriptor=d)}
    for ex in (b'',b'\xfe\xca\x00\x00',b'\xfe\xca\x01\x00z',b'\x01\x00\x00\x00',b'\xfe\xca\xff\xff',b'\0'):
        yield {'data':zip_bytes(files,local_extra=ex)}
        yield {'data':zip_bytes(files,central_extra=ex)}
    for name in ['', '../x','a/b/c','.hidden','a\\b','a/','a//b','/root','a\x00b','a\x7fb','a\u0085b','x\u202ey','a\u200db','x\U000e0001y',b'\xc0\x80',b'\xed\xa0\x80','Привет.content','C:/data','a.\u0301','doc/..\u0301','.\u0301hidden','a/\u0301b','a/\u0301b/c','a\\\u0301b']:
        yield {'data':zip_bytes([(name,b'test',0)])}
    for a,b in [('é.rm','e\u0301.rm'),('가.rm','가.rm'),('a\u0301\u0323','a\u0323\u0301'),('a','a')]:
        yield {'data':zip_bytes([(a,b'x',0),(b,b'y',0)])}
    for n in range(len(seed)): yield {'data':seed[:n]}
    for n in range(len(seed)):
        data=bytearray(seed); data[n]^=255; yield {'data':bytes(data)}
    defaults=[65535,4294967295,2**63-1,200,1<<20,4294967295,2**63-1,2**63-1,65535]
    for i in range(9):
        for value in [0,1,10,100,1000]:
            limits=defaults.copy(); limits[i]=value
            for selection in [None,[],['doc.content']]:
                case={'data':seed,'limits':limits}
                if selection is not None: case['select']=selection
                yield case
                yield dict(case,list=True)
    rng=random.Random(0x524d444f43)
    for i in range(1000):
        data=bytearray(seed)
        for _ in range(rng.randrange(1,6)): data[rng.randrange(len(data))]=rng.randrange(256)
        yield {'data':bytes(data),'list':bool(i%2)}
    # Overlap: place a complete valid local record inside another stored payload.
    inner=zip_bytes([('inner',b'z',0)]); idir=s.unpack_from('<I',inner,len(inner)-6)[0]
    outer=bytearray(zip_bytes([('outer',inner[:idir],0)]))
    odir=s.unpack_from('<I',outer,len(outer)-6)[0]
    record=bytearray(inner[idir:-22]); s.pack_into('<I',record,42,35)
    old=outer[odir:-22]; body=outer[:odir]
    yield {'data':bytes(body+old+record+s.pack('<IHHHHIIH',0x06054b50,0,0,2,2,len(old)+len(record),odir,0))}
