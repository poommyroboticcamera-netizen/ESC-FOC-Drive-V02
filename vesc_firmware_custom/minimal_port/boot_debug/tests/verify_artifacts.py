"""Verify BIN/HEX identity, vector sanity, ELF target and startup call order."""
import hashlib,json,struct
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'output'
binary=(p/'custom_v02_boot_debug.bin').read_bytes()
elf=(p/'custom_v02_boot_debug.elf').read_bytes()
assert elf[:4]==b'\x7fELF' and elf[4]==1 and elf[5]==1
assert struct.unpack_from('<H',elf,18)[0]==40, 'ELF machine must be ARM'
sp,reset=struct.unpack_from('<II',binary)
assert sp==0x20020000 and reset&1
assert 0x08000000 <= (reset&~1) < 0x08000000+len(binary)
upper=0; memory={}; eof=False
for line in (p/'custom_v02_boot_debug.hex').read_text().splitlines():
    assert line.startswith(':')
    r=bytes.fromhex(line[1:]); assert sum(r)%256==0
    n,a,t=r[0],int.from_bytes(r[1:3],'big'),r[3]
    assert len(r)==n+5
    if t==0:
        for i,b in enumerate(r[4:4+n]): memory[upper+a+i]=b
    elif t==4: upper=int.from_bytes(r[4:6],'big')<<16
    elif t==1: eof=True
assert eof and min(memory)==0x08000000
recovered=bytes(memory.get(0x08000000+i,0) for i in range(len(binary)))
assert recovered==binary and max(memory)==0x08000000+len(binary)-1
asm=(p/'disassembly.txt').read_text(encoding='utf-8-sig')
startup=asm.split('<Reset_Handler>:',1)[1].split('<Default_Handler>:',1)[0]
assert startup.index('<board_gate_safe_early>') < startup.index('<main>')
report={'status':'PASS','scope':'static artifacts and host MMIO tests; no hardware run',
        'binary_bytes':len(binary),'initial_sp':hex(sp),'reset_vector':hex(reset),
        'mailbox_address':'0x20000000','hex_matches_bin':True,
        'sha256':hashlib.sha256(binary).hexdigest()}
(p/'artifact_verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
