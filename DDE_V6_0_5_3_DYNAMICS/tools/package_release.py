"""Versioned release, explicit file inventory and byte-for-byte ZIP verification."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT=Path(__file__).resolve().parents[1]
PREFIX='OJM_DDE_V6_0_5_2_DYNAMICS'
EXCLUDED={'.venv','.build','__pycache__','build','.git'}
ROOT_FILES={'.gitignore','AGENTS.md','README.md','CAN_PROTOCOL.md','VALIDATION.md','FILES_CHANGED.md','CONTINUE_HERE_CZ.md','CHANGELOG.md','CHOREOGRAPHY_CZ.md','SHA256SUMS_V6_0_1.txt','SHA256SUMS_V6_0_2.txt','SHA256SUMS_V6_0_3.txt','SHA256SUMS_V6_0_4.txt'}
DIRS={'OJM_DDE_Dynamics','Vehicle_ECU_Simulator','assets','tests','tools','validation','firmware'}
ROOT_FILES.add('SHA256SUMS_V6_0_5.txt')
ROOT_FILES.add('SHA256SUMS_V6_0_5_1.txt')

def files():
    result=[]
    for file in ROOT.rglob('*'):
        if not file.is_file():continue
        rel=file.relative_to(ROOT)
        if any(part in EXCLUDED for part in rel.parts) or file.suffix=='.pyc':continue
        if rel.parts[0] in DIRS or (len(rel.parts)==1 and file.name in ROOT_FILES):result.append(file)
    return sorted(result,key=lambda p:p.relative_to(ROOT).as_posix())

def package(output=None):
    output=Path(output) if output else ROOT.parent/'releases'/f'{PREFIX}.zip'
    # Refuse accidental overwrite of the explicitly preserved baseline releases.
    if output.name!=f'{PREFIX}.zip':raise ValueError('Use the dedicated V6.0.5.2 release filename')
    output.parent.mkdir(parents=True,exist_ok=True)
    report_file=ROOT/'validation/package_check.json'
    report_file.write_text('{}\n')
    inventory=ROOT/'FILES_CHANGED.md'
    inventory.write_text('')
    paths=files()
    listing=['# Soubory nové revize V6.0.5.2','',
        'Všechny níže uvedené soubory jsou v samostatné složce DDE_V6_0_5_2_DYNAMICS.',
        'V6.0.1 zůstala beze změny: validation/integrity_audit.json. V5.6.1 nebyla v tomto workspace přítomna.',
        'Nový archiv je releases/OJM_DDE_V6_0_5_2_DYNAMICS.zip. Předchozí validace zůstává v původních složkách revizí.',
        'Lokální .venv, build a cache nejsou součástí distribuovaného projektu.','']
    listing.extend(f'- `{p.relative_to(ROOT).as_posix()}`' for p in paths)
    listing.append('- `SHA256SUMS.txt` (manifest ostatních souborů)')
    inventory.write_text('\n'.join(listing)+'\n',encoding='utf-8')
    manifest=ROOT/'SHA256SUMS.txt'
    required=['OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino','OJM_DDE_Dynamics/src/TitanAssets.h',
        'OJM_DDE_Dynamics/src/DdeFonts.h','firmware/OJM_DDE_V6_0_5_2_merged.bin','Vehicle_ECU_Simulator/main.py',
        'Vehicle_ECU_Simulator/dist/OJM_DDE_Virtual_ECU.exe','tools/convert_assets.py',
        'README.md','CAN_PROTOCOL.md','VALIDATION.md']
    names={p.relative_to(ROOT).as_posix() for p in paths}
    assert all(name in names for name in required),'Incomplete source/release inventory'
    for name in ('CanTransport.h','DdeProtocol.h','DynamicsModel.h','DdeRenderer.h',
                 'BaselineGraphics.h','BaselineShapes.h','BaselineInput.h','SmartDial_Fonts_DINish.h','OjmLogo.h','DdeNavigation.h','ArtFont.h','ArtTop.h','ArtLogo.h','TitanStrokes.h','LocalPresentation.h'):
        assert f'OJM_DDE_Dynamics/src/{name}' in names,name
    assert 'OJM_DDE_Dynamics/build_opt.h' in names
    def write_and_verify():
        manifest.write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(ROOT).as_posix()+'\n' for p in paths))
        with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
            for p in [*paths,manifest]:z.write(p,f'{PREFIX}/{p.relative_to(ROOT).as_posix()}')
        with zipfile.ZipFile(output) as z:
            assert z.testzip() is None
            assert len(z.namelist())==len(paths)+1
            for p in [*paths,manifest]:assert z.read(f'{PREFIX}/{p.relative_to(ROOT).as_posix()}')==p.read_bytes(),p
    write_and_verify()
    report_file.write_text(json.dumps({'status':'PASS','archive_entries':len(paths)+1,
        'all_entries_byte_equal_to_source':True,'zip_crc_checked':True,'required_files_present':True,
        'baseline_archives_excluded':True,'environments_and_caches_excluded':True},indent=2)+'\n')
    # Rebuild once to include the completed validation report, then check again.
    write_and_verify()
    digest=hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.zip.sha256').write_text(digest+'  '+output.name+'\n')
    print(json.dumps({'archive':str(output),'entries':len(paths)+1,'bytes':output.stat().st_size,'sha256':digest,'verification':'PASS'},indent=2))

if __name__=='__main__':package()
