# V6.0.7 — ověření

Python: 37 testů PASS (protokol, CommandEngine, worker, nové Qt GUI testy proti DemoBus: synchronizace, zámky se
neodešlou, Drift jen v Track a návrat na Sport, Launch/Boost, výpadek signálu, scénář, offline náhled). gui_smoke
(offscreen Qt) PASS se snímky validation/gui_*.png při 1600×920, 1280×720 a 1920×1080, fonty Clash Display / Sora /
Michroma ověřené. Startup ve Studiu: časy 6490 ms / ∞ zavřené v 1034 ms jako firmware. Firmware: jen verze; host
render test a kompilace viz níže. Fyzický CAN se Studiem (CANable) neověřen.

# V6.0.6.9 — ověření

Host: 4145 snímků == čisté rendery (nová sekvence `locks`: narážka na zamčené RWS Off nad 120 km/h a na Drift mimo
Track s nápovědou), startup fast == reference, stall test, 35 Python testů PASS. Build: flash 682 926 B,
RAM 150 436 B. Deska (COM4, Serial): výběr TV 59,0 fps, narážky 59,2–59,4, late=0. Vzhled uživatel zatím neviděl.

# V6.0.6.8 — ověření

Host: 4133 snímků == čisté rendery (sekvence `action` na stránce RWS: přechod do bočního pohledu, světla, Go s
proudnicí, Boost se smyčkou proudnic, odchod otočením); startup fast == reference, stall test, 35 Python testů,
GUI smoke PASS. Build: flash 681 618 B, RAM 150 372 B. Deska (COM4, Serial simulace): Launch 58,9 fps, Go 59,4–59,8,
Boost 59,3 (jeden snímek 23 ms při startu), odchod 59,4, late=0. Vzhled uživatel zatím neviděl.

# V6.0.6.7 — ověření

Host: 4335 snímků (UI + 330 startup) == čisté rendery, včetně nové sekvence `action` (hold prstence, Launch
zap/zrušení/Go, Boost a odpočet, Auto polohy podle rychlosti/brzdy, zámky Off nad 120 km/h a Drift mimo Track,
odmítnutí vzdáleného příkazu ACK 1, návrat bez dat); startup fast == reference, stall test 298 snímků, coverage 6M,
35 Python testů, GUI smoke PASS. Audit: hystereze enkodéru a debounce beze změny, žádný původní řádek CAN workeru
nezměněn. Build: flash 679 126 B, RAM 150 356 B.
Deska (COM4, Serial simulace auta): late=0; Launch zap 59,4 fps, zrušení 58,8, Boost 59,4, Air Brake 59,4, RWS 59,4,
selektor 58,0 (souběh zavření selektoru a změny Auto 1 s ~46 fps). Vzhled na displeji uživatel zatím neviděl.
Fyzický CAN rámec 0x100 neověřen (Studio ho ještě neposílá), jen Serial simulace.

# V6.0.6.6 — ověření

Host: 2868 UI + 330 startup snímků == čisté rendery, startup fast == reference, stall test 298 snímků, coverage 6M,
GUI smoke PASS; 35 Python testů PASS (časově citlivý test reálného 60 Hz cyklu napoprvé 54/60 při souběhu jiných
testů, napodruhé PASS; PC aplikace beze změny kromě verze). Build: flash 672 346 B, RAM 150 212 B.
Deska (COM4): uživatel potvrdil — tečka po laseru pryč, Titan při zoomu nekříží prstenec; SHADOW_CHECK během držení
loga: 0 rozdílných px, 0 zahozených/odmítnutých přenosů; Aero zoom 59 fps, RWS 59, menu 56, late=0.

# V6.0.6.5 — ověření

Host: 2868 UI + 330 startup snímků == čisté rendery (pásy řádků, pool z více oblastí, PSRAM přesah), startup fast ==
reference, stall test 298 snímků, coverage 6M, 35 Python testů, GUI smoke PASS. Startup snímky se od 6.0.6.4 liší
(5bitová alfa míchání, odstíny AA). Build: flash 672 142 B, RAM 150 172 B.
Deska (COM4, Serial): příkazy přes nové zařízení potvrzeny TE (TEOFF/TEON), přenosové cesty testovacími čtverci
(viděl uživatel: knihovna, jednotlivé transakce, SCT s příkazy po jedné lince ano; SCT s quad příkazy ne).
UI late=0, Aero zoom 59 fps, RWS 59, menu 56, startup infinity+laser 56,5 fps (fáze plnění ~44 fps). CAN fyzicky
v této revizi neměřen.


Host: 2868 UI + 330 startup snímků == čisté rendery přes pool v interní RAM; stall a coverage testy jdou cestou
bez poolu (PSRAM); 1441 snímků sekvencí pixelově shodných s 6.0.6.3; AA, 35 Python testů, GUI smoke PASS.
Míchání bez dělení ověřeno úplně (64×64×256 kanálů, 256×256 násobení alfy). Fyzicky neověřeno.


Host: 2868 UI + 330 startup snímků == čisté rendery; GIF sekvence (1441 snímků) pixelově shodné s 6.0.6.2;
stall test, AA cesty, coverage 6M, 35 Python testů, GUI smoke PASS. Build -O2: flash 638 270 B, RAM 74 292 B.
Fyzicky neověřeno.


Host: 2868 UI + 330 startup snímků == čisté rendery (rychlá cesta QSPI: převod po blocích, pořadí bufferů,
big-endian a kurzor okna dekódovány do paměťového panelu; cesta knihovny ověřena vzorkovanými přenosy CO5300),
stall test 298 snímků, AA cesty shodné, coverage 6M, 35 Python testů, GUI smoke. Build: flash 623 562 B,
RAM 74 196 B (+16 KB DMA bufferů za běhu). Průměrné px/snímek: RWS 21 426, TV 19 042, Aero 21 772,
přechody 35 320 (6.0.6.1: 47 935 / 48 597 / 44 401 / 40 371). Fyzicky neověřeno (FPS, FAST_QSPI na panelu).


Vzhled beze změny: všechny host testy níže znovu PASS (2868 UI + 330 startup snímků == čisté rendery,
stall test 298 snímků, AA cesty shodné, coverage 6M). Startup max 46 512 px/snímek (6.0.6: 82 604).
Vnitřní smyčky kreslení na snímek (host čítač, renderer_test.txt `work`): prstenec ~5–7k místo
celých ploch obdélníků. Build: flash 616 170 B, RAM 68 876 B; firmware/OJM_DDE_V6_0_6_1_merged.bin.
Fyzicky neověřeno — čeká na Serial report uživatele. Níže původní protokol 6.0.6.


## Skutečný renderer — PASS

render_test.cpp kompiluje skutečné firmware headers (vstup, navigace, model,
startup, Halo 2 UI, renderer) do WASI a spouští smyčku po 0,5 ms se simulovaným
TE 16,6 ms (záměrně ne nominálních 16,667 ms). 2826 odeslaných snímků: panel ==
framebuffer == čistý render celé scény v čase snímku. Každý přenos začíná na TE
hraně, žádná hrana nedostane dva snímky, perioda TE se naučí na 16 607 µs.
renderer_test.txt/json, GIFy a kontaktní archy firmware_*.png/gif.

- CO5300: sudé x/y/w/h před restore/draw/pack, stride, clipping, překryv,
  dotyk oken zůstává oddělený, přetečení seznamu, pásy prstence (73 228 px).
- Vstup: 2000 detentů, částečné hrany, změna směru, dávky, debouncovaný klik+otočení.
- Selektor: kandidát, odeslání po 250 ms klidu (ne dřív), CAN stav, rychlé točení
  neodešle mezilehlé režimy, odraz na dorazu, klik odešle, dlouhý stisk zruší,
  8 s auto-zavření; RWS pořadí 3,1,2,0 s CAN hodnotami.
- Vzdálené příkazy: změna stránky během výběru odešle volbu a následuje,
  otevřený selektor zrcadlí vzdálenou změnu režimu; ACK cesta.
- Klidné UI nepřenáší nic (200 kroků, 0 px).

Startup: 6 490 ms, ∞ uzavřené v 1 034 ms, míjení v 0,65 s, laser do 2 750 ms.
331 snímků == čisté rendery; hold odeslán jednou; poslední snímek Titanu je
v oblasti auta pixelově shodný s prvním snímkem UI. Prodlevy smyčky 9/17/33/50/65/
250 ms: 122 snímků shodných, přenosy stále na TE hraně (startup_gaps.txt).
compare_startup.py: rychlá a referenční AA cesta pixelově shodné ve všech
331 snímcích (startup_comparison.json, startup_v606_*.csv).
Coverage: 6 milionů vzorků hran přesně shodných se sqrtf (coverage_test.txt).

## Přenosový rozpočet (host, px/snímek — NE FPS)

| Sekvence | průměr | max |
|---|---|---|
| startup | 12 324 | 82 604 |
| menu (přechody stránek) | 39 955 | 120 104 |
| Aero (zoom, křídlo) | 44 876 | 196 168 |
| RWS | 48 203 | 136 456 |
| TV | 48 805 | 136 456 |

Celý panel (217 156 px) jen při přeskočení startupu. Host/WASI časy nejsou FPS
ESP32. Poslední fyzické měření je z 5.6 (v6056_hardware.json): přenos i kreslení
na desce jsou drahé, proto se odesílají jen skutečně změněné oblasti. 6.0.6 zatím
není fyzicky změřená: po nahrání ověřit FPS (Serial), enkodér a obousměrný CAN.

## PC — PASS

35 Python/CAN testů (python_tests.txt), včetně Halo 2 náhledu: délka startupu,
∞ v 0,65 s, bílé logo v holdu, Aero po Titanu, změna režimu otevře výběr, zoom
na křídlo, zavření, přechody všech stránek. GUI DEMO: gui_smoke.json/gui_run.txt,
snímky gui_startup/gui_standalone/gui_0..2/gui_small/gui_diagnostics.png;
Michroma přes skutečný Tk font (font_check.json); 1080×720 i 900×620 bez ořezu.
Náhled kreslí ~5 ms/snímek. EXE se v 6.0.6 nestaví (nebylo požadováno).

## Firmware / integrita

Finální build (firmware_build.txt): ESP32-S3 core 3.3.10, CDCOnBoot=cdc,
FlashSize=8M, huge_app, PSRAM=opi. Flash 613 874 B / 3 145 728 B, statická RAM
64 996 B / 327 680 B (geometrie a časová tabulka startupu). export_firmware.py:
21 sestavených headers == zdroje (firmware_source_match.json); 8 MB obraz
firmware/OJM_DDE_V6_0_6_merged.bin, offset 0x0. Nic neflashováno ani nepushováno.

audit_release.py (integrity_audit.json): všechny historické manifesty včetně 5.8
beze změny; proti 5.8 se změnily jen zamýšlené soubory; CanTransport.h,
DdeProtocol.h, BaselineInput.h, BaselineGraphics.h, FastCoverage.h, piny/TE/QSPI,
CAN_PROTOCOL.md a PC CAN moduly byte-identické. Byte-for-byte ZIP a CRC:
package_check.json. V5.6.1 zde není dostupná.
