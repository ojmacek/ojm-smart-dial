# OJM DDE V6.0.7 — CANertia Studio

V6.0.7: PC appka = CANertia Studio v PySide6 (main.py, studio_theme.py tokeny/fonty, studio_widgets.py, studio_paint.py
QPainter pro startup, startup.py geometrie startupu z firmwaru, torus_rules.py zrcadlo TUNING, scenario.py data scénáře).
Tk soubory (halo.py, controls.py, theme.py, ui_order.py, spec, BUILD_EXE.bat) jsou pryč. Barvy a fonty jen přes tokeny ve
studio_theme.py; kobalt střídmě, žádné záře. Startup ve Studiu mění se jen spolu s firmwarem (stejné vzorce).
TUNING v torus_rules.py musí odpovídat TorusContext.h. Clash Display nikdy do release/GitHubu (licence ITF FFL).
Každý snímek: Studio.tick (8 ms timer) → události z workeru → model auta → pravidla → scénář → sync → animate jen
pohyblivých widgetů. Nikdy neblokovat UI vlákno; CAN jen ve workeru.

# OJM DDE V6.0.6.9 — viditelné zámky

V6.0.6.9: narážka na zamčenou volbu (nav_lock_seq/page/pos v DdeNavigation.h) → HaloUi ukáže 0,8 s její název šedě
a důvod (torus_lock_reason v TorusContext.h) v řádku pod názvem (UI_HINT 380–408, ui_slots[18]). Zamčený segment
UI_LOCKED, plná délka.

# OJM DDE V6.0.6.8 — akční pohled

V6.0.6.8: akční pohled (HaloUi.h, ui_ax = čistá funkce času z ui.axOn/axAt/axFrom): Launch, 900 ms po Go a Boost
(dokud ho nezruší otočení/výběr) ukazují boční pohled na každé stránce. Prolnutí pohledů je postupné (horní ven v první
půlce, boční dovnitř v druhé) — kvůli výkonu, souběžné kreslení obou pohledů dávalo na desce ~45–49 fps. Akční
proudnice mají vlastní sloty ui_slots[6]/[17], světla ui_slots[0]. Startup ani CAN se nemění.

# OJM DDE V6.0.6.7 — Performance Torus

V6.0.6.7: `src/TorusContext.h` = situace (stav vozidla 0x100, Drive Mode z 0x202), zámky voleb, co volí Auto,
rychlá akce long pressu v menu (Launch/Boost). Všechny prahy a časy jsou v `TUNING` — koncept se může měnit, měň
tabulku, ne logiku. Bez dat = stojící auto. Pořadí RWS je nově Off, Auto, Stable, Agile (CAN 0,3,1,2; mode_position
už není involuce, inverze je mode_value). TV: Off, Auto, Sport, Drift (CAN čísla beze změny). Zamčené volby musí
zůstat na krajích seznamu (navigace jen zužuje rozsah). Plný prstenec rychlé akce se kreslí přes art_ring_stack,
částečný jako oblouk: přechod mezi cestami poškodí celý pás prstence (jinak zůstanou staré pixely). Když při vstupu
do UI už akce běží, vstup překreslí celou obrazovku místo adopce Titanu. BaselineInput.h: jen čtecí g_button_down/
g_button_down_at, hystereze a debounce beze změny (audit to kontroluje). CanTransport.h: přibyly dva přijímané ID,
žádný původní řádek se nezměnil (audit). Studio zatím neumí 0x100 ani nové názvy (další krok).

# OJM DDE V6.0.6.6 — Halo 2

V6.0.6.6: rozhodnutí podle času nesmí stát na přesné rovnosti float součtů (deska počítá s FMA jinak než host:
konec laseru je od trace_end). Radiální prolnutí čar (art_fade_set/off, per jádro) se zapíná jen kolem bočního
pohledu při zoomu. Každý přenos pixelů = jeden segment SCT.

# OJM DDE V6.0.6.5 — Halo 2

V6.0.6.5: po setupu FastQspi.h (qspi_takeover) uvolní zařízení i sběrnici knihovny a nastaví vlastní (stejné piny,
mód 0, 80 MHz, HW CS na TFT_CS); knihovnu potom NEPOUŽÍVEJ (display->… kreslení, set_brightness). Panel bere v SCT
jen příkazy 0x02 po jedné lince (QSPI_SPLIT 1); příkazy 0x32 s quad parametry nefungují (ověřeno čtverci
QSPI_SELFTEST). Kreslení = úlohy (pásy řádků) na obou jádrech; vše, co scéna cachuje, se plní v teplém průchodu
s prázdným clipem před paralelní částí. Obdélník se posílá hned po dokreslení (send_advance), proto se pool smí
přepsat až po transfer_wait_idle(). Tabulky startupu se po startupu půjčují poolu: startup se na desce nesmí
spustit znovu.


V6.0.6.4: všechny pixely scény jdou přes halo_px() do halo_target (pool v interní RAM nebo PSRAM rámec); nový
primitiv nesmí volat blend_pixel(g_frame,...) přímo. Host test porovnává panel s čistým renderem (rámec už
není zrcadlem panelu).

V6.0.6.3: `#pragma GCC optimize("O2")` v .ino pro celý kreslicí kód; HALO_COUNT/halo_work počítá iterace na
desce (Serial `ui:` work). Řádkové úseky čar (line_row_span) jen pro boxy širší než LINE_RUN_MIN (12 px).

V6.0.6.2: přenos na panel FastQspi.h (DMA, dva interní buffery, převod v QspiStream.h; soukromý handle knihovny
čten standardním idiomem explicitní instanciace, knihovna se nemění). FAST_QSPI 0 = přenos knihovny.
Poškozené oblasti jsou podél čar (DamageSlot na chunk/pás/kolo/segment); nové prvky musí mít vlastní box,
host test (čistý render každého snímku) je jediné kritérium správnosti. DIRTY_MAX 48; u plného seznamu se
překryvy vždy slučují (jinak hrozí nekonečné dělení).

V6.0.6.1 = výkon 6.0.6 beze změny vzhledu. Snímek začíná hned po předchozím (průměrný odhad kreslení,
ne nejhorší snímek); Serial `ui:` report. Měření 6.0.6 na desce: startup draw 10,5 ms, TE čekání 7,4 ms.

V6.0.6 nasazuje uživatelem schválený návrh Halo 2 (design/halo2-ui-2026-10-20,
SPEC_HALO2_CZ.md) a nový schválený startup „Nekonečno → laser“ (varianta 10).
Před změnou čti README.md, CONTINUE_HERE_CZ.md, CHOREOGRAPHY_CZ.md, THEME_CZ.md,
CAN_PROTOCOL.md a VALIDATION.md. FIX_INPUT_CZ.md je historie vstupní opravy 5.4.
Starší revize a archivy zachovej. V5.6.1 je stabilní baseline, zde není dostupná.
Nic neflashuj a nepushuj bez zadání uživatele.

Startup (HaloStartup.h): dvě kuličky se stuhou z obou okrajů, každá jednu půlku ∞
konstantní rychlostí 0,5 px/ms; kulička B zrychlí přes spojku do O na laser
1,72 px/ms, obkreslí všechny obrysy loga s jiskrami, celé logo se rozsvítí bíle
(850 ms), hold 0,9 s, zavření do tečky 0,8 s, Titan 1,1 s; celkem 6,49 s.
Konec Titanu je pixelově totožný s prvním snímkem UI (test). Startup měnit jen se
souhlasem uživatele. compare_startup.py vyžaduje shodu obou AA cest na všech snímcích.

UI (HaloUi.h) je port prototypu 1:1: vizuální pořadí Aero, RWS, TV (CAN ID stránek
beze změny). Aero zoom 2,75× na zadní třetinu, křídlo jako lícující profil obrysu
bez vzpěry a stupnice; RWS kola, barva fáze, záblesk průchodu nulou, stopy a živá
stopa; TV hladiny momentu s nalitím a odpichem. Zamítnuto uživatelem: ozvěna,
válec, modrý nádech v autě, 3D, OEM úprava, deformace karoserie.

Navigace (DdeNavigation.h): otočení v menu mění stránku hned; v selektoru se mění
jen kandidát a odešle se po 250 ms klidu; klik odešle a zavře, dlouhý stisk zavře
bez odeslání, 8 s bez vstupu zavře. Dial musí fungovat bez ECU.

Každý snímek: přesné označení změněných položek (DamageSlot + otisk vstupů),
restore z base, překreslení po obdélnících, přenos hned po TE. Panel musí být po
každém snímku shodný s čistým renderem — render_test.cpp to kontroluje.
CO5300 dirty region má sudé x/y/w/h; rozšíření předchází restore/draw/pack.
Prstenec se poškozuje po pásech; překrývající se okna se ořezávají, ne slučují.
Takt je zamčený na TE (frame_due) a scéna se vzorkuje v čase zobrazení. Výkon měřit
host čítači (budget/work v renderer_test.txt) a Serial reportem na desce, ne host časy.

FastCoverage.h vyžaduje test_coverage.py (6M hran), kalibraci na desce před animací
a původní sqrtf fallback. Nedělat závěry o FPS podle hostu.
Žádné runtime PNG/SVG, blokující animace ani alokace v render smyčce.
Serial report až po animaci, jen do dostupného TX prostoru.

CAN, piny, TE, QSPI a core0 worker zachovej. RWS AUTO=3, STABLE=1, AGILE=2, OFF=0,
vizuální pořadí 3,1,2,0. Boční displeje nejsou implementované.
BaselineInput.h: symetrická hystereze jedné hrany při 4 hranách na krok, int64
pending, bez resetu při kliku/změně směru; klik+otočení: nejdřív kontext.
Debounce 24 ms, long press 700 ms.

Jeden hlavní .ino + src/. Slučování do jediného .ino je samostatný požadavek.
Generované headers needitovat ručně: HaloAssets.h ← tools/build_halo_assets.py,
WebFonts.h ← tools/build_theme.py, ArtTop.h/TitanStrokes.h ← build_artistic_assets.py,
SignatureAssets.h ← build_signature_assets.py; PC logo ← tools/build_pc_assets.py.

PC (Vehicle_ECU_Simulator/halo.py) je Python port stejného startupu a UI; EXE se
v 6.0.6 nestaví (nebylo požadováno). Ověření: test_renderer.py, test_coverage.py,
compare_startup.py, test_startup_gaps.py, Python unittest, gui_smoke.py --capture,
finální Arduino compile, export_firmware.py, audit_release.py, package_release.py.
Host test, GIF a DEMO neprokazují fyzický CAN, enkodér ani FPS na Waveshare.
