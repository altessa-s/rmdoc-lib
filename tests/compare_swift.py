#!/usr/bin/env python3
# Copyright 2026 ALTESSA SOLUTIONS INC.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.
"""Cross-check against a local rmdoc-swift checkout (macOS Swift toolchain).
Build shared first; then: python3 tests/compare_swift.py --swift PATH --library LIB
"""
import argparse, base64, ctypes as c, json, pathlib, subprocess, tempfile
from cases import cases
class Str(c.Structure): _fields_=[('bytes',c.c_void_p),('length',c.c_size_t)]
class Bytes(c.Structure): _fields_=[('bytes',c.c_void_p),('size',c.c_size_t)]
class File(c.Structure): _fields_=[('name',Str),('data',Bytes)]
class Archive(c.Structure): _fields_=[('files',c.POINTER(File)),('count',c.c_size_t),('storage',c.c_void_p)]
class Entry(c.Structure): _fields_=[('name',Str),('compressed',c.c_uint32),('uncompressed',c.c_uint32),('crc',c.c_uint32),('method',c.c_uint16)]
class Limits(c.Structure): _fields_=[('v'+str(i),c.c_uint64) for i in range(9)]
Select=c.CFUNCTYPE(c.c_uint32,c.c_void_p,c.POINTER(Entry),c.POINTER(c.c_bool))
def raw(x): return c.string_at(x.bytes,x.length)
def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--swift',required=True,type=pathlib.Path); parser.add_argument('--library',required=True); args=parser.parse_args()
    lib=c.CDLL(args.library)
    lib.rmd_reader_open.argtypes=[Bytes,c.POINTER(Limits),c.POINTER(c.c_void_p),c.c_void_p]; lib.rmd_reader_open.restype=c.c_uint32
    lib.rmd_reader_close.argtypes=[c.c_void_p]
    lib.rmd_reader_count.argtypes=[c.c_void_p]; lib.rmd_reader_count.restype=c.c_size_t
    lib.rmd_reader_entry.argtypes=[c.c_void_p,c.c_size_t]; lib.rmd_reader_entry.restype=c.POINTER(Entry)
    lib.rmd_reader_unpack.argtypes=[c.c_void_p,Select,c.c_void_p,c.POINTER(Archive),c.c_void_p]; lib.rmd_reader_unpack.restype=c.c_uint32
    lib.rmd_pack.argtypes=[c.POINTER(File),c.c_size_t,c.POINTER(Bytes),c.c_void_p]; lib.rmd_pack.restype=c.c_uint32
    lib.rmd_archive_free.argtypes=[c.POINTER(Archive)]; lib.rmd_buffer_free.argtypes=[c.POINTER(Bytes)]
    def run(case):
        data=c.create_string_buffer(case['data']); reader=c.c_void_p()
        limits=Limits(*case['limits']) if 'limits' in case else None
        status=lib.rmd_reader_open(Bytes(c.addressof(data),len(case['data'])),c.byref(limits) if limits else None,c.byref(reader),None)
        if status: return {'status':status}
        try:
            if case.get('list'):
                return {'status':0,'names':[raw(lib.rmd_reader_entry(reader,i).contents.name).decode() for i in range(lib.rmd_reader_count(reader))]}
            def select(ctx,entry,include):
                include[0]='select' not in case or raw(entry.contents.name).decode() in case['select']; return 0
            archive=Archive(); status=lib.rmd_reader_unpack(reader,Select(select),None,c.byref(archive),None)
            if status: return {'status':status}
            try:
                result={'status':0,'names':[],'payloads':[]}
                for i in range(archive.count):
                    f=archive.files[i]; result['names'].append(raw(f.name).decode()); result['payloads'].append(base64.b64encode(c.string_at(f.data.bytes,f.data.size)).decode())
                if archive.count:
                    packed=Bytes(); rc=lib.rmd_pack(archive.files,archive.count,c.byref(packed),None)
                    assert rc==0,rc
                    result['packed']=base64.b64encode(c.string_at(packed.bytes,packed.size)).decode(); lib.rmd_buffer_free(c.byref(packed))
                return result
            finally: lib.rmd_archive_free(c.byref(archive))
        finally: lib.rmd_reader_close(reader)
    with tempfile.TemporaryDirectory(prefix='rmdoc-crosscheck-') as tmp:
        exe=pathlib.Path(tmp)/'crosscheck'
        subprocess.run(['swiftc','-module-cache-path',str(pathlib.Path(tmp)/'module-cache'),'-O','-module-name','RMDocCrosscheck',*[str(p) for p in sorted((args.swift/'Sources/RMDoc').glob('*.swift'))],str(pathlib.Path(__file__).parent/'swift-crosscheck/main.swift'),'-o',str(exe)],check=True)
        inputs=list(cases())
        lines='\n'.join(json.dumps(dict(x,data=base64.b64encode(x['data']).decode())) for x in inputs)+'\n'
        output=subprocess.run([str(exe)],input=lines,text=True,capture_output=True,check=True).stdout.splitlines()
        assert len(output)==len(inputs)
        for i,(case,line) in enumerate(zip(inputs,output)):
            expected=json.loads(line); actual=run(case)
            if actual!=expected:
                raise SystemExit(f'case {i}: expected {expected}, got {actual}; input={base64.b64encode(case["data"]).decode()}')
        print(f'{len(inputs)} Swift/C cross-check cases passed (statuses, selection, contents, stored byte parity).')
if __name__=='__main__': main()
