"""Self-contained notebook handoff: current source tree + preserved releases."""
from pathlib import Path
import hashlib
import json
import zipfile
from package_release import ROOT, PREFIX

def transfer():
    release_dir=ROOT.parent/'releases'
    current=release_dir/f'{PREFIX}.zip'
    if not current.exists():raise FileNotFoundError('Run package_release.py first')
    candidates=['OJM_Smart_Dial_V5_6_CAN_FAST_baseline.zip','OJM_Smart_Dial_V5_6_1_CAN_FAST.zip','OJM_DDE_V6_0_DYNAMICS.zip','OJM_DDE_V6_0_1_DYNAMICS.zip']
    previous=[name for name in candidates if (release_dir/name).is_file()]
    missing=[name for name in candidates if name not in previous]
    target=release_dir/'OJM_DDE_V6_0_7_NOTEBOOK.zip'
    expected={}
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as out:
        with zipfile.ZipFile(current) as source:
            for name in source.namelist():
                # Friendly directory matches the actual project name on the new machine.
                renamed=name.replace(PREFIX+'/',ROOT.name+'/',1)
                data=source.read(name);out.writestr(renamed,data);expected[renamed]=hashlib.sha256(data).hexdigest()
        for name in previous:
            path=release_dir/name;data=path.read_bytes();key='previous_releases/'+name
            out.writestr(key,data,compress_type=zipfile.ZIP_STORED);expected[key]=hashlib.sha256(data).hexdigest()
        note=(f'Začni souborem {ROOT.name}/CONTINUE_HERE_CZ.md.\n'
              'Celý archiv je lokální předávací balík. Google Drive upload provádí uživatel.\n'
              'Chybějící staré archivy (nejsou součástí tohoto balíku): '+', '.join(missing)+'\n')
        out.writestr('START_HERE.txt',note.encode('utf-8'));expected['START_HERE.txt']=hashlib.sha256(note.encode('utf-8')).hexdigest()
    with zipfile.ZipFile(target) as check:
        assert check.testzip() is None
        assert set(check.namelist())==set(expected)
        for name,digest in expected.items():assert hashlib.sha256(check.read(name)).hexdigest()==digest,name
    digest=hashlib.sha256(target.read_bytes()).hexdigest()
    target.with_suffix('.zip.sha256').write_text(digest+'  '+target.name+'\n')
    print(json.dumps({'archive':str(target),'entries':len(expected),'bytes':target.stat().st_size,'sha256':digest,'verification':'PASS'},indent=2))

if __name__=='__main__':transfer()
