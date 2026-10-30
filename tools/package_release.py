"""Versioned release, explicit file inventory and byte-for-byte ZIP verification."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT=Path(__file__).resolve().parents[1]
PREFIX='OJM_DDE_V6_0_7_DYNAMICS'
EXCLUDED={'.venv','.build','__pycache__','build','.git'}
# Clash Display (ITF Free Font License) may be used by Studio but not redistributed as a file: kept out of the archive.
NOT_REDISTRIBUTABLE=('ClashDisplay-',)
ROOT_FILES={'.gitignore','AGENTS.md','README.md','CAN_PROTOCOL.md','VALIDATION.md','FILES_CHANGED.md','CONTINUE_HERE_CZ.md','CHANGELOG.md','CHOREOGRAPHY_CZ.md','SHA256SUMS_V6_0_1.txt','SHA256SUMS_V6_0_2.txt','SHA256SUMS_V6_0_3.txt','SHA256SUMS_V6_0_4.txt'}
DIRS={'OJM_DDE_Dynamics','Vehicle_ECU_Simulator','assets','tests','tools','validation','firmware'}
ROOT_FILES.update({'SHA256SUMS_V6_0_5.txt','SHA256SUMS_V6_0_5_7.txt'})
ROOT_FILES.update({'SHA256SUMS_V6_0_5_1.txt','SHA256SUMS_V6_0_5_2.txt','GITHUB_STATUS_CZ.md'})
ROOT_FILES.update({'FIX_INPUT_CZ.md','SHA256SUMS_V6_0_5_6.txt','THEME_CZ.md'})
ROOT_FILES.update({'SHA256SUMS_V6_0_5_4.txt','SHA256SUMS_V6_0_5_5.txt'})
ROOT_FILES.update({'SHA256SUMS_V6_0_5_8.txt','SHA256SUMS_V6_0_6.txt','SHA256SUMS_V6_0_6_1.txt','SHA256SUMS_V6_0_6_2.txt','SHA256SUMS_V6_0_6_3.txt','SHA256SUMS_V6_0_6_4.txt','SHA256SUMS_V6_0_6_5.txt','SHA256SUMS_V6_0_6_6.txt','SHA256SUMS_V6_0_6_7.txt','SHA256SUMS_V6_0_6_8.txt','SHA256SUMS_V6_0_6_9.txt'})

def files():
    result=[]
    for file in ROOT.rglob('*'):
        if not file.is_file():continue
        rel=file.relative_to(ROOT)
        if any(part in EXCLUDED for part in rel.parts) or file.suffix=='.pyc':continue
        if file.name.startswith(NOT_REDISTRIBUTABLE) and file.suffix in ('.otf','.ttf','.woff','.woff2'):continue
        if rel.parts[0] in DIRS or (len(rel.parts)==1 and file.name in ROOT_FILES):result.append(file)
    return sorted(result,key=lambda p:p.relative_to(ROOT).as_posix())

def package(output=None):
    output=Path(output) if output else ROOT.parent/'releases'/f'{PREFIX}.zip'
    # Refuse accidental overwrite of the explicitly preserved baseline releases.
    if output.name!=f'{PREFIX}.zip':raise ValueError('Use the dedicated V6.0.7 release filename')
    output.parent.mkdir(parents=True,exist_ok=True)
    report_file=ROOT/'validation/package_check.json'
    report_file.write_text('{}\n')
    inventory=ROOT/'FILES_CHANGED.md'
    inventory.write_text('')
    paths=files()
    listing=['# Soubory nové revize V6.0.7 (CANertia Studio: nová PC aplikace v Qt ve stylu webu OJM Systems)','',
        'Všechny níže uvedené soubory jsou v samostatné složce DDE_V6_0_7_DYNAMICS.',
        'V6.0.1 zůstala beze změny: validation/integrity_audit.json. V5.6.1 nebyla v tomto workspace přítomna.',
        'Nový archiv je releases/OJM_DDE_V6_0_7_DYNAMICS.zip. Předchozí validace zůstává v původních složkách revizí.',
        'Lokální .venv, build a cache nejsou součástí distribuovaného projektu.','']
    listing.extend(f'- `{p.relative_to(ROOT).as_posix()}`' for p in paths)
    listing.append('- `SHA256SUMS.txt` (manifest ostatních souborů)')
    inventory.write_text('\n'.join(listing)+'\n',encoding='utf-8')
    manifest=ROOT/'SHA256SUMS.txt'
    # V6.0.6: the PC mirror runs from source (no EXE was requested).
    required=['OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino','firmware/OJM_DDE_V6_0_7_merged.bin','Vehicle_ECU_Simulator/main.py',
        'Vehicle_ECU_Simulator/startup.py','Vehicle_ECU_Simulator/studio_theme.py','Vehicle_ECU_Simulator/studio_widgets.py','tools/build_halo_assets.py','tools/build_pc_assets.py','tools/convert_assets.py',
        'README.md','CAN_PROTOCOL.md','VALIDATION.md','tools/build_signature_assets.py',
        'assets/source/logo_contours.json','assets/preview/halo_logo.png','assets/source/approved_logo_mask.png',
        'assets/source/logo_velke_cernybg_bilytext.png','OJM_DDE_Dynamics/src/WebFonts.h','OJM_DDE_Dynamics/src/FastCoverage.h',
        'Vehicle_ECU_Simulator/torus_rules.py','Vehicle_ECU_Simulator/scenario.py','assets/fonts/Sora-Variable.ttf','assets/fonts/Sora-OFL.txt','assets/studio/canertia_studio.png','assets/studio/ojmsystems_web.png','assets/fonts/Michroma-Regular.ttf','assets/fonts/Michroma-OFL.txt','THEME_CZ.md']
    names={p.relative_to(ROOT).as_posix() for p in paths}
    assert all(name in names for name in required),'Incomplete source/release inventory'
    for name in ('CanTransport.h','DdeProtocol.h','DynamicsModel.h','DdeRenderer.h',
                 'BaselineGraphics.h','BaselineShapes.h','BaselineInput.h','SmartDial_Fonts_DINish.h','DdeNavigation.h','ArtTop.h','TitanStrokes.h',
                 'AssetTypes.h','QspiStream.h','FastQspi.h','HaloAssets.h','HaloDraw.h','HaloStartup.h','HaloUi.h','SignatureAssets.h','SignaturePerf.h','WebFonts.h','FastCoverage.h','DevBench.h','TorusContext.h'):
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
