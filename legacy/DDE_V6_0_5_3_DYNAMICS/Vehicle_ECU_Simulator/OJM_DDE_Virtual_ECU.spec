# -*- mode: python ; coding: utf-8 -*-
a=Analysis(['main.py'],pathex=[],binaries=[],
    datas=[('../assets/preview','assets/preview'),('OJM_Logo.png','.')],
    hiddenimports=['can.interfaces.slcan'],hookspath=[],hooksconfig={},runtime_hooks=[],excludes=[],noarchive=False)
pyz=PYZ(a.pure)
exe=EXE(pyz,a.scripts,a.binaries,a.datas,[],name='OJM_DDE_Virtual_ECU',
    debug=False,bootloader_ignore_signals=False,strip=False,upx=True,console=False,
    disable_windowed_traceback=False)
