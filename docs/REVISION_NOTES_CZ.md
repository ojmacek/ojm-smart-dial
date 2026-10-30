# OJM DDE V6.0.7 — CANertia Studio

PC aplikace je přepsaná: **CANertia Studio** (Python + PySide6/Qt místo Tkinteru), anglicky, ve stylu chystaného webu
OJM Systems: grafit #14171C, plochy #1B2028, měkká bílá #F3F1EB, vedlejší text #B4BDC9, kobalt #0038FF střídmě;
nadpisy Clash Display, ostatní text Sora, Michroma (font firmwaru) jen na „displejích“ Torusů. Jemné hranice, lehce
zaoblené prvky, decentní stíny, krátké plynulé přechody (pružiny jako na Torusu). Logo OJM Systems, odkaz
ojmsystems.com a nápis CANertia Studio jsou dodané grafiky beze změny (assets/studio, assets/preview/ojm_logo.png).
- **Startup** stejný jako na Torusu (∞ → laser → logo → Titan; geometrie a časy z firmwaru, kreslí QPainter), pod
  logem nápis CANertia Studio; klik/Esc přeskočí; pak UI postupně najede.
- **Vehicle**: rychlost (posuvník, rychlé volby, šipky ↑/↓), Park / Drive / Charging, Brake, Signal dropout. Auto se
  k cílové rychlosti rozjíždí/brzdí pevnou rychlostí (VehicleModel.step — připraveno na pozdější fyziku plynu/brzdy).
  Studio posílá stav vozidla 0x100 (20 Hz). Přehled „What the Torus sees“ = stejná pravidla jako firmware
  (torus_rules.py zrcadlí TUNING z TorusContext.h): stání/jízda, co udělá long press, zámek Off, Drift, Auto volby.
- **Karty Torusů** Energy · Drive · Performance (pořadí jako v krabičce): živý „displej“ Performance (stránka, režim,
  auto s křídlem, Launch/Boost prstenec a světla, akční texty), volby s klouzající kobaltovou destičkou; zamčené volby
  se neodešlou (odraz + důvod), odmítnutí Torusem (ACK 1) se ukáže. Drive Torus Studio simuluje (0x202, tlačítko
  Simulate) do doby, než bude Torus 3; Energy je „next“.
- **Scenario**: 13 kroků obhajoby s anglickými popisky (scenario.py, data), Play/Pause/Restart, časová osa; kroky
  „Hold the knob“ čekají na Launch/Boost bit z Torusu (v demu se knob stiskne sám).
- **CAN log** (tlačítko vpravo nahoře / L): čitelné řádky (VEHICLE …, ACK …, TORUS AERO -> …), Save/Clear.
- `main.py --demo`: DemoBus je teď in-memory Performance Torus (zámky, ACK 1, Auto, Launch/Boost přes „Hold knob
  (demo)“). `--port COM8` = CANable (slcan). `--no-startup` přeskočí animaci.
- Fonty: Sora (OFL) je přibalená. **Clash Display (ITF Free Font License) se smí používat, ale ne šířit jako soubor**:
  je v assets/fonts jen lokálně, release ZIP ji vynechává (package_release.py), bez ní nadpisy padnou na Soru.
  Na GitHub ji nedávat (.gitignore).
- Firmware beze změny kromě verze.

# OJM DDE V6.0.6.9 — Performance Torus: viditelné zámky

Uživatel k 6.0.6.8: „co je ta blank space ve vectoringu?“ — byl to zamčený Drift (jen v Drive Mode Track), kreslený
skoro neviditelně. V6.0.6.9:
- Zamčená volba = šedý tenký segment plné délky (ne tmavý a krátký), takže je vidět, že tam je, jen teď nejde.
- Když knob narazí na zamčenou volbu, na 0,8 s se místo názvu režimu šedě ukáže její název a pod ním důvod:
  „Track only“ (Drift), „Above 120 km/h“ (RWS/TV Off; text se skládá z TUNING.off_lock_kmh).
- Odemknutí Driftu na desce bez Drive Torusu: Serial `k` třikrát (→ Track).
- Deska: výběr TV 59,0 fps, narážka 59,4, late=0.

# OJM DDE V6.0.6.8 — Performance Torus: akční pohled

Uživatel k 6.0.6.7: model auta při Launch a Boostu „nedotažený“, a ať se na jakékoli stránce ukáže totéž.
V6.0.6.8 (schváleno: varianta „akční pohled“):
- Launch i Boost na **každé stránce** přepnou na boční pohled auta: horní pohled nejdřív zmizí, pak se objeví boční
  (každý snímek kreslí jen jeden pohled; na Aero stránce auto prostě zůstane). Texty se mění stejně postupně.
  Lišta zvoleného režimu dole se v akčním pohledu skryje.
- **Launch:** tři startovní světla nad autem se po zapnutí rozsvítí jedno po druhém, při rozjezdu zhasnou naráz
  („lights out“) a po karoserii přeběhne proudnice; 0,9 s „Go“, pak návrat na stránku. Zrušení = návrat hned.
- **Boost:** proudnice po karoserii ve smyčce (nová každých 550 ms, ke konci každých 1250 ms a pomalejší).
  Prstenec zůstane 300 ms plný (během přechodu), pak odpočítává. Otočení knobem nebo otevření výběru během Boostu
  akční pohled ukončí, Boost dál běží jen na prstenci.
- Deska (Serial `ui:`, late=0): Launch na stránce RWS 58,9 fps, Go 59,4–59,8, Boost 59,3, odchod otočením 59,4.

# OJM DDE V6.0.6.7 — Performance Torus (CANertia)

Téma BP: chytrý ovladač pro automotive, obecné sportovní EV (ne Titan). Tři Torusy v jedné krabičce: Energy,
Drive (master), Performance. Tento firmware je **Performance Torus**. Koncept dohodnutý s uživatelem (může se ještě
měnit, proto jsou prahy a časy v jedné tabulce `TUNING` v `src/TorusContext.h`): „Torus pozná situaci“.

Funkce (vizuální pořadí; CAN hodnoty beze změny):
- Active Aero: Auto · Low Drag · Downforce · Air Brake. **Auto** volí polohu z rychlosti: pomalu Low Drag, od 80 km/h
  Downforce, při brzdění ≥40 % nad 60 km/h Air Brake (hystereze). Bez dat o autě vypadá Auto jako dřív (12°).
- Rear Wheel Steering: **Off · Auto · Stable · Agile** (CAN 0, 3, 1, 2). Auto: pod 60 km/h Agile, nad Stable.
- Torque Vectoring: **Off · Auto · Sport · Drift** (CAN 0–3 jako dřív Off/Normal/Sport/Max). Drift jen v Drive Mode
  Track (Drive Torus, 0x202); když Track skončí nebo Drive Torus zmlkne, TV přejde na Sport.
- Zámky: RWS Off a TV Off nejdou zvolit nad 120 km/h (odemknutí pod 115). Zamčená volba je v selektoru tenký tmavý
  segment a knob se před ní zastaví s odrazem (zamčené volby jsou vždy na kraji). Vzdálený příkaz na zamčenou
  hodnotu dostane ACK status 1 a neprovede se.
- **Long press v menu** (dřív nic): halo se plní 700 ms (ukáže se až po 120 ms, takže běžný klik ho nikdy neukáže).
  Auto stojí → **Launch** („Launch Control / Ready“, prstenec svítí; klik/otočení zruší, rozjezd ≥5 km/h = „Go“,
  po 30 s se zruší). Auto jede ≥20 km/h → **Boost** 10 s („Boost / Active“, prstenec se odpočítává), pak 20 s pauza
  (long press = šedé naplnění, nic se nestane). Long press ve výběru dál = zrušit bez odeslání.
- Bez rámce vozidla se Torus chová jako stojící auto (bezpečný výchozí stav): vše jako dřív, jen Boost není.

CAN (viz CAN_PROTOCOL.md): nový rámec stavu vozidla 0x100 (Studio/auto → všechny Torusy), Drive Mode čtený
z 0x202, bajt 5 stavu 0x201 = bit0 Launch armed, bit1 Boost. CANertia Studio zatím neposílá 0x100 ani nezná nové
názvy TV/pořadí RWS (úprava Studia je další krok); pro zkoušení na desce Serial: '0'–'9' rychlost ×25 km/h,
'b' brzda, 'p' nabíjení, 'k' Drive Mode (žádný → Comfort → Sport → Track), 'x' konec simulace; 'l' long press.

Měření na desce (Serial `ui:`, late=0): Launch zap 59,4 fps, zrušení 58,8, Boost 59,4, Auto Air Brake 59,4,
RWS 59,4, selektor 58,0. Plný prstenec jde stejnou tabulkovou cestou jako puls výběru (art_ring_stack).

# OJM DDE V6.0.6.6 — Halo 2 (opravy po 6.0.6.5)

Uživatel k 6.0.6.5: „jede to, TOP smooth“, ale (1) po dokreslení posledního písmene zůstane tečka, (2) modrá linka
prstence při zoomu jede přes Titana. V6.0.6.6:
- Zazoomovaný Titan (boční pohled, odraz, křídlo, proudnice) měkce mizí v pásu r 192–204 px kolem středu, takže
  nekříží prstenec ani segmenty. Pás najíždí se zoomem zvenku (bez zoomu auto sahá do r ~196 a kreslí se beze změny).
- Konec laseru: od konce dráhy je obrys vždy hotový. Na desce součty s FMA vyšly těsně pod tour_len, q zůstalo
  0,99999 a hlava laseru svítila na posledním písmenu po celé plnění loga (host dával přesně 1, proto test nic neviděl).
- Přenos: každý přenos pixelů je jeden segment; větší okna jdou jako několik oken celých řádků (pokračování 0x3C00
  po uvolnění CS mezi segmenty se nepoužívá).
- Vývojové: SHADOW_CHECK 1 v .ino porovná na desce vše odeslané s čistým renderem během držení loga (Serial).
- Výkon stejný jako 6.0.6.5 (Aero zoom 59 fps, late=0).

# OJM DDE V6.0.6.5 — Halo 2 (výkon 5)

Měření 6.0.6.4 na desce (Serial trace po snímcích): Aero zoom se vstupním pulsem prstence 30 fps
(~92k px, kreslení 18–21 ms + QSPI 8–9 ms za sebou), startup fáze plnění loga 34 fps (29 ms na jednom jádře).
V6.0.6.5 (změřeno na desce, Serial `ui:`/`trace`, late=0 = každý snímek na své TE hraně):
- Kreslení na obou jádrech: obdélníky si berou obě jádra ze společného čítače; velké obdélníky (auto, logo ve
  startupu) po pásech řádků. Startup kreslí také na obou jádrech (stejné pixely).
- Přenos překrytý s kreslením: obdélníky jsou seřazené shora dolů a každý jde na panel hned, jak je hotový a přišla
  jeho TE hrana, zatímco se dokresluje zbytek (DMA je rychlejší než skenování panelu). Po setupu se sběrnice
  nastaví znovu se stejnými piny, módem a taktem, vlastní zařízení s HW CS na stejném pinu a SCT (segmentované
  transakce ESP32-S3): okno = 1 transakce s příkazy (0x02, jedna linka, jako knihovna) + 1 s pixely (0x32 quad).
  Ověřeno na panelu testovacími čtverci (QSPI_SELFTEST 1): příkazy s parametry po 4 linkách panel NEbere.
- Pool v interní RAM ~101k px: zbytek heapu nad rezervu 48 KB a geometrické tabulky startupu (po startupu mrtvé).
- Prstenec (puls) celočíselně z jedné tabulky (35/46 taktů na px místo 58/173), skleněný oblouk jako první prvek
  scény (obnova obdélníku = memset, žádné čtení základu z PSRAM), míchání 5bitovou alfou v jednom 32bit slově.
- Puls při vstupu do výběru běží během pohybu Aero zoomu po krocích 30 Hz (snímky střídavě ~18/10 ms → s překryvem
  každá hrana má snímek, zoom plných 60 fps). Jinde puls 60 Hz.
- Výsledky: Aero zoom 59 fps, zoom ven 57–58, intro prstence 59, RWS editace 59, přechod stránek 56 (crossfade
  ~8 snímků po ~22 ms), startup plnění loga ~44 fps (22,6 ms; pás řádků 264–280 je na desce nejdražší).
- Vzhled: tvary a časování beze změny; 5bitová alfa mění odstíny AA hran nepatrně (startup snímky už nejsou
  bitově shodné s 6.0.6.4, fast i reference cesta jsou spolu shodné). Vývojové Serial příkazy: + - c l (vstup),
  B (bench), T (trace snímků), R (obdélníky), J (pásy jader).


Měření 6.0.6.3: paint 8–20 ms při 8–25 tisících iteracích = ~0,9 µs (~210 taktů) na krok smyčky → kreslení
do PSRAM a dělení 255 při každém míchání barvy.
V6.0.6.4 (vzhled pixelově beze změny):
- Každý poškozený obdélník se kreslí do vlastního bufferu v interní RAM (pool 64k px = 128 KB, menší při nedostatku
  paměti) a na panel jde přímo odtud; PSRAM se při kreslení nepoužije (obdélníky nad kapacitu poolu jako dřív).
- Míchání barev bez dělení (halo_blend565/halo_mul): přesně stejné výsledky pro všechny vstupy (ověřeno úplně).
- Serial `ui:` ukazuje i `pool=` (podíl pixelů kreslených v interní RAM).


Měření 6.0.6.2: QSPI ~0,2 µs/px (dříve 0,3), kreslení ~0,5 µs/px, 28–42 fps; nejhorší Aero zoom.
V6.0.6.3 (vzhled pixelově beze změny, všech 1441 snímků sekvencí shodných s 6.0.6.2):
- Čáry: u dlouhých čar se na řádku navštíví jen úsek, kterého se čára může dotknout (zoom: segmenty jsou 2,75× delší).
- Oblouky: test úhlu vektorovým součinem místo atan2 na pixel, plné jádro bez odmocniny, přesný box bez vzorkování.
- Prstenec: pásy s optimalizovanými hranicemi (puls 54k → 48k px, vlasová čára 45k → 38k).
- Kreslicí kód překládán s -O2 (Arduino výchozí -Os).
- Serial `ui:` nově ukazuje rozpad kreslení: upd (stav+škody), rest (obnova pozadí), paint, work (iterace smyček).


Měření 6.0.6.1 na desce: UI ~0,4 µs kreslení + ~0,3 µs QSPI na každý přenesený pixel, 35–130k px/snímek.
V6.0.6.2 (vzhled pixelově beze změny):
- Rychlý přenos na panel (FAST_QSPI): DMA ze dvou interních bufferů, převod přímo z framebufferu
  (bez kopie do push bufferu), stejné příkazy, piny, takt i TE. `#define FAST_QSPI 0` v .ino vrací přenos knihovny.
- Křídlo: řádkové vyplnění místo testu 4 vzorků × 34 hran na pixel (Aero zoom).
- Poškozené oblasti podél čar: auto po skupinách 16 úseků (i odraz), horní pohled po pásech, kola zvlášť
  vpředu/vzadu, každý segment a lišta zvlášť, prstenec podle druhu změny. Průměr UI: RWS 48k → 21k px,
  TV 49k → 19k, Aero 44k → 22k, přechody stránek 40k → 35k (max 118k → 104k).
- Levnější kreslení: přeskakování skupin úseků mimo obdélník, řádkový index obrysu, text bez dělení na pixel,
  černé plochy mazané memset místo kopie z PSRAM, šířky textů zapamatované.


V6.0.6.1 opravuje výkon 6.0.6 na desce beze změny vzhledu (host testy pixel po pixelu):
časování podle TE (6.0.6 čekala ~7–9 ms na snímek), levnější prstenec, logo, stuha a odraz,
žádné celoplošné překreslení při změně fáze startupu ani při vstupu do UI, Serial report UI.

Nasazený schválený návrh Halo 2 a nový startup „Nekonečno → laser“ (6,49 s).
Firmware i PC aplikace mají stejný styl a stejné animace. Navazuje na V6.0.5.8.

## Co je nové

- Startup: ∞ kreslené dvěma kuličkami, laser obkreslí logo, bílé rozsvícení,
  hold, tečka, Titan. Titan přechází do UI bez jediného změněného pixelu.
- UI Halo 2: tenký prstenec (šedý = menu, modrý = výběr režimu, puls při otevření),
  značky stránek nahoře, segmenty režimů dole, název stránky 20 px a režim 40 px.
- Active Aero: klik přiblíží zadní třetinu 2,75×, křídlo je lícující profil obrysu.
- RWS: natáčená kola, modrá = fáze, záblesk při průchodu nulou, stopy, živá stopa.
- Torque Vectoring: hladiny momentu na kolech, nalití a odpich.
- Výběr režimu: kandidát se odešle po 250 ms klidu (rychlé točení neposílá
  mezilehlé režimy), klik odešle a zavře, dlouhý stisk zruší, 8 s auto-zavření.
- Plynulost: přesné poškozené oblasti, takt zamčený na TE a vzorkování scény
  v okamžiku zobrazení; klidné UI nepřenáší nic.
- PC aplikace: stejný startup i UI (Python + Pillow, 3× supersampling), paleta Halo 2.

## Spuštění

- Arduino: OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino + src. ESP32S3 Dev Module,
  240 MHz, 8 MB, Huge APP, PSRAM OPI, USB CDC Enabled, core 3.3.10.
- Firmware: firmware/OJM_DDE_V6_0_6_2_merged.bin, offset 0x0 (zatím nenahráno).
- PC: `python Vehicle_ECU_Simulator/main.py --demo` nebo `--port COM8`
  (START.bat / DEMO.bat). Klik na živý dial přeskočí startup. EXE se nestaví.
- ZIP: ../releases/OJM_DDE_V6_0_6_2_DYNAMICS.zip.

Otočení listuje Aero → RWS → Vectoring. RWS pořadí AUTO/STABLE/AGILE/OFF má
CAN hodnoty 3/1/2/0. Dial plně funguje bez ECU; PC je volitelné ovládání a zrcadlo.

Serial Monitor 115200 po startupu: řádky infinity+laser / close / Titan
(fps, draw, TE, QSPI) a AA backend; pak každé 3 s při pohybu řádek `ui:` (fps, draw,
max, TE, QSPI, px, rects, late). Fyzické FPS 6.0.6.2 ještě nejsou změřené.
Testy a integrita: VALIDATION.md. Nic se automaticky neflashuje ani nepushuje.
