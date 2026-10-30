# V6.0.5.2 — oprava dílčích přenosů CO5300

Uživatel na fyzickém displeji pozoroval posunuté kusy auta a zbytky dalších
scén. V6.0.5.1 posílala liché souřadnice a rozměry dílčích oken. Nainstalovaný
Arduino_GFX předává tyto hodnoty řadiči bez zarovnání. CO5300 potřebuje sudá
počáteční x/y i šířku/výšku. Tento požadavek uvádí také referenční implementace
[ovladače CO5300 v Zephyru](https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/display/display_co5300.c)
ve funkci co5300_copy_and_adjust_coordinates. Hash kontrolovaných místních
souborů Arduino_GFX je v validation/inspected_driver_hashes.json.

Jde o konkrétní nalezený nesoulad s požadavky panelu a pravděpodobné vysvětlení
fotografií. Oprava je ověřená v kódu; výsledek na zařízení zatím fyzicky ověřený
není. Test nesimuluje přesný vzor poškození panelu.

## Oprava

dirty() zarovná okno směrem ven před slučováním, obnovou podkladu, kreslením
a zabalením pixelů. Okno i stride odpovídají stejným rozměrům. Například
animace kol 143,143,181,47 se odešle jako 142,142,182,48. Grafika se neposune:
rozšíření pouze přidá okolní pixely. BaselineGraphics.h, TE, QSPI rychlost,
piny, CAN, model, animace i assets jsou beze změny.

## Regrese a renderer — PASS

- Před opravou kontrola selhala už v 16 ms na startup okně 50,130,368,155.
  Log: validation/alignment_before_fix.txt.
- Po opravě každý přenos splňuje sudé x/y/w/h a hranice panelu.
- 22 přenosů s pixelovým vzorem ověřuje stride, všechny kombinace parity,
  problémová okna, okraje panelu i zachování okolí. Další případy ověřují
  prázdná okna, slučování a zaplnění dirty seznamu.
- 3 358 snímků: přenesený panel shodný s framebufferem a čistým renderem;
  startup/skip, rychlé listování, všechny lokální režimy a nezávislost na ECU.
- 101 šířek odkrytí loga zachovává pevnou velikost, hold je 1 s.
- 512 fází Vectoringu neprotíná bílou siluetu. Rail, dorazy, poslední volba
  i odložená proudnice dál procházejí původními kontrolami.
- Maximum při vstupu do scény: 151 032 pixelů proti původním 149 850 (+0,79 %),
  nikoli všech 217 156 pixelů panelu. Maximum testované interakce Vectoringu
  je 80 052 pixelů. Ustálené animace nepřekračují 31 000 pixelů.

Skutečné firmware headers testovány přes Zig / WebAssembly / WASI. Logy:
validation/renderer_test.txt a renderer_run.txt. Exporty PNG/GIF obsahují
stejný renderer; vizuálně zkontrolovány RWS, TV a Aero. Jde o kontroly pixelů
a objemu přenosu, nikoli fyzických FPS nebo CAN latence.

## Firmware — PASS

Kompilace prošla: 499 782 B flash / 3 145 728 B, 25 252 B statické RAM.
Výsledek sestavení: validation/firmware_build.txt. Stejné nastavení:
esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=huge_app,PSRAM=opi.
Export tools/export_firmware.py kontroluje kopie všech kompilovaných headers
proti zdrojům (pouze CRLF a Arduino #line). Výsledek a SHA-256 obrazu:
validation/firmware_source_match.json. Merged obraz 8 MB je pro offset 0x0.

## Integrita a PC

Python ani EXE se nemění a znovu se nesestavují. Audit porovnává všechny
PC soubory včetně EXE s V6.0.5. Předchozí ověření 35 Python testů, GUI a EXE
je ve V6.0.5/VALIDATION.md, nejde o nový běh pro tuto opravu. Audit kontroluje
manifesty všech původních revizí včetně V6.0.5.1, neměněné firmware moduly,
CAN a top-view originál. V5.6.1 zde není dostupná.
Report: validation/integrity_audit.json.

Balík OJM_DDE_V6_0_5_2_DYNAMICS.zip obsahuje zdroj a firmware. Kontroluje se
CRC i bajtová shoda všech položek: validation/package_check.json. Manifest:
SHA256SUMS.txt. Staré revize i archivy zůstávají zachované.

## Ověření na skutečném dialu — zbývá

Nic nebylo flashováno. Po nahrání V6.0.5.2 ověřit:

1. Celý startup a jeho přeskočení otočením/kliknutím.
2. Opakovaně Aero → RWS → TV v obou směrech, pomalu i rychle. Po přepnutí
   nesmí zůstávat kusy bočního auta, přerušené obrysy ani přesunuté proužky.
3. Všechny režimy a opakované browse/edit, nejdřív bez PC.
4. Obousměrné volby přes CAN s PC a následné odpojení PC.
5. Změřit FPS, tearing a CAN latenci; >=30 FPS z host testu nelze potvrdit.
