# OJM DDE V6.0.5.3 — rozpracovaná verze

Aktuální zdroje obsahují opravu přechodu browse/edit a pořadí RWS
AUTO → STABLE → AGILE → OFF v dialu i Python ovládání. CAN hodnoty se nemění.
Tato revize zatím nemá dokončenou validaci ani nový firmware/EXE.
Převod všech vlastních částí firmwaru do jediného .ino ještě není proveden.
Aktuální stav a další kroky: GITHUB_STATUS_CZ.md.

Následující popis a odkazy na binární soubory patří ověřené V6.0.5.2
v sousední složce DDE_V6_0_5_2_DYNAMICS, nikoliv hotovému vydání V6.0.5.3.

## Převzatý základ V6.0.5.2

Tato oprava řeší rozpad obrazu na fyzickém CO5300: oblasti pro dílčí přenos
se před kreslením zarovnají na sudé souřadnice a rozměry. Starší test tuto
podmínku nekontroloval. Nový test ji kontroluje při každém přenosu.
Podrobnosti a omezení ověření: VALIDATION.md.

Dial funguje samostatně bez ECU a PC. Tato revize doplňuje schválenou
choreografii, nový minimalistický pohled shora a jemné reakce ovládání.
Výchozí stránka zůstává Active Aero / Downforce.

## Spuštění

- Arduino: otevři OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino. Hlavní kód je
  v jednom tabu; pomocné moduly a grafická data jsou ve složce src/.
  Malý build_opt.h v kořeni obsahuje nastavení kompilátoru.
- Deska: ESP32S3 Dev Module, 240 MHz, 8 MB, Huge APP, PSRAM OPI, USB CDC Enabled.
- Připravený firmware: firmware/OJM_DDE_V6_0_5_2_merged.bin, kompletní obraz
  pro offset 0x0. Nic nebylo automaticky nahráno do zařízení.
- Volitelné PC ovládání: Vehicle_ECU_Simulator/dist/OJM_DDE_Virtual_ECU.exe.
  Zdroje lze spustit přes INSTALL.bat a START.bat. COM8 je jen výchozí volba.
  --demo používá paměťovou sběrnici, žádný fyzický CAN port.

## Co je nové

Startup trvá 3,71 s. Dvě tečky se během 470 ms spojí, logo se rozbalí,
jednu sekundu zůstane ve středu a sbalí se zpět do bodu. Ten vyletí a postupně
vykreslí Titan. Křídlo se usadí a stejný obraz pokračuje v Active Aero.

Oprava převzatá z V6.0.5.1: rozbalení je odkrývání svislých pruhů od středu ven jako
v dodané Climate verzi. Písmena mají po celou dobu stejnou velikost i polohu.
Otevření trvá 300 ms a zavření od okrajů do středu 280 ms, hold zůstává 1 s.
Předpočítané zmenšeniny loga byly odstraněné; nic se nezvětšuje ani nezmenšuje.

Otočení listuje přímo Aero → Rear Wheel Steering → Torque Vectoring.
Klik otevře výběr režimu: tři stránkové segmenty se přemění na čtyři režimové,
zvednou o 5 px a červený bod se plynule přesouvá podle enkodéru.
Další klik potvrdí a vrátí listování. Podržení je návrat, nikdy reset.
Rychlé otáčení ihned mění cílovou volbu; animace se nestaví do fronty.

RWS a Vectoring používají nový dodaný obrys. RWS ukazuje mechanické natáčení,
červený akcent zadních kol a krátké vnější oblouky. Vectoring má emisní body,
krátké pulzy bez trvalých spojnic a reakci příslušného kola. Pulzy neprotínají
bílý obrys. Normal, Sport a Max mají jiné zatížení, tempo a sílu; Max přidá
slabší druhý pulz. Off energii nechá dohasnout.

Aero má pevný pivot křídla a jednu krátkou proudnici. Nejprve se změní křídlo,
proudění naváže s odkladem 700 ms. Stopa projede za 900 ms jednou za 3,2 s.
Low Drag odchází rovně, Downforce dolů, Air Brake se rozdělí a Auto reaguje
na okamžitou polohu. Air Brake je přímo volitelný i bez ECU.

Přesná choreografie a mapa kódu pro obhajobu: CHOREOGRAPHY_CZ.md.

## PC a CAN

Python aplikace je pouze druhé ovládání a náhled. Výběr TV MAX na PC pošle
stránku i režim do dialu; otočení dialu se objeví na PC. Připojení/odpojení
PC nemění lokální animace. Offline volby PC se po připojení nepřehrávají.
Žádná simulační telemetrie se z běžné aplikace neposílá.

PC animace má vlastní hodiny a krátké zvýraznění právě zvoleného režimu.
CAN sdílí režimy a stránku, ne fázi animace ani místní browse/edit stav.
CAN v2, ID, DLC, checksum, sequence ACK a worker jsou shodné s V6.0.4.
Přiložená appka zůstává beze změny V6.0.5; nový je pouze firmware V6.0.5.2.
Boční displeje a jejich společný startup se zatím neimplementují.

## Rozsah ověření

VALIDATION.md obsahuje skutečné výsledky sestavení a testů.
Model je ilustrační HMI, ne měření aktuátorů vozidla.
Renderer přenáší změněné oblasti; cílová smyčka je 60 Hz.
Nejméně 30 FPS, tearing a fyzický CAN je nutné ověřit na skutečném Waveshare.

Předchozí revize a ZIPy zůstaly zachované. V5.6.1 zde není dostupná,
zůstává stabilní baseline. Font licence jsou přiložené.
