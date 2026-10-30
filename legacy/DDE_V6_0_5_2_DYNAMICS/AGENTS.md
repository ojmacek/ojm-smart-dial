# OJM DDE V6.0.5.2

Tato revize opravuje zarovnání dílčích přenosů CO5300. Choreografie je převzatá.
Před další prací čti README.md, CONTINUE_HERE_CZ.md, CHOREOGRAPHY_CZ.md,
CAN_PROTOCOL.md a VALIDATION.md. V6.0.1–5.1 i jejich archivy zachovej.
V5.6.1 zůstává stabilní baseline, v tomto workspace není.

Dial vždy funguje plnohodnotně bez ECU. Lokální model řídí všechny režimy
a animace. Python je volitelné druhé ovládání / mirror. CAN telemetrie
neřídí vizuál. Žádná synchronizace bočních displejů v této revizi.

Plné názvy stránek; přímé listování, klik browse/edit, čtyři režimy.
Startup: pomalejší setkání teček, skutečné rozbalení loga, hold 1 s,
sbalení do bodu, kreslení Titanu, usazení křídla. Rozbalení znamená odkrývání
svislých pruhů od středu při pevné velikosti písmen, nikdy zoom.
Viz CHOREOGRAPHY_CZ.md. PC aplikace je beze změn převzatá V6.0.5.
Nový top-view assets/source/titan_top_minimal.png zachovej beze změny.
Příprava malého render assetu probíhá offline, originál se nepřepisuje.

Hlavní Arduino tab je OJM_DDE_Dynamics.ino, moduly a data jsou v src/.
Piny, CO5300, TE, encoder primitives, core0 CAN task, latest-wins,
sequence ACK a heartbeat zachovej. CAN v2 je beze změny proti V6.0.4.
Font licence zachovej. Žádný runtime PNG/SVG nebo blokující animace.
CO5300 dirty region musí mít sudé x/y/w/h. Rozšiřovat před restore/draw,
nikdy měnit jen adresní okno bez přepočtu stride. BaselineGraphics.h neměněn.

Validace cíleně, bez opakování beze změn: C++ renderer/model/navigace
tools/test_renderer.py; Python py_compile a unittest; vizuální GUI kontrola.
Finální Arduino compile, export_firmware.py, audit_release.py a package_release.py.
Host snímky, DEMO a simulovaný ACK nejsou fyzické FPS/CAN měření.
Neflashuj bez zadání uživatele.
