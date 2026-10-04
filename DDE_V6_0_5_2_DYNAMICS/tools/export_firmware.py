"""Export actual Arduino build after checking the copied compile inputs."""
from pathlib import Path
import hashlib, json, shutil
ROOT=Path(__file__).resolve().parents[1]
def run():
    hashes={}
    for source in (ROOT/'OJM_DDE_Dynamics').rglob('*.h'):
        relative=source.relative_to(ROOT/'OJM_DDE_Dynamics')
        compiled=(ROOT/'.build/firmware/sketch'/relative).read_bytes()
        if compiled.startswith(b'#line 1 '):compiled=compiled.split(b'\n',1)[1]
        assert compiled.replace(b'\r\n',b'\n')==source.read_bytes().replace(b'\r\n',b'\n'),source
        hashes[relative.as_posix()]=hashlib.sha256(source.read_bytes()).hexdigest()
    ino=ROOT/'OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino'
    hashes[ino.name]=hashlib.sha256(ino.read_bytes()).hexdigest()
    src=ROOT/'.build/firmware/OJM_DDE_Dynamics.ino.merged.bin'
    assert src.stat().st_size==8388608
    dst=ROOT/'firmware/OJM_DDE_V6_0_5_2_merged.bin';shutil.copy2(src,dst)
    report={'status':'PASS','compiled_headers_match_final_sources':True,
            'comparison':'Arduino #line directive and CRLF normalization only',
            'source_sha256':hashes,'merged_sha256':hashlib.sha256(dst.read_bytes()).hexdigest()}
    (ROOT/'validation/firmware_source_match.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS: {len(hashes)-1} compiled headers; exported {dst.name}')
if __name__=='__main__':run()
