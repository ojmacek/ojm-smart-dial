# Pokračování — V6.0.7 (CANertia Studio)

Uživatel: „přepracuj úplně Studio, ať je stejně smooth a clean jako UI na displeji“; potom styl podle chystaného webu
OJM Systems (barvy, Clash Display/Sora, Michroma kde se hodí, loga beze změny). Odsouhlaseno: PySide6, rozvržení
(Vehicle vlevo, karty Energy · Drive · Performance, scénář dole, tlačítko CAN log), zjednodušené ovládání auta
(rychlost + stav; fyzika později), vše anglicky. Studio běží z demo režimu i proti CANable; fyzicky s deskou zatím
nevyzkoušeno. Další kroky: zpětná vazba k vzhledu, ověření s deskou přes CANable (0x100 → Auto/zámky na displeji),
fyzika plynu/brzdy, pak jádro firmwaru + role (Energy, Drive).

# Pokračování — V6.0.6.9 (viditelné zámky)

Uživatel se ptal na prázdné místo ve výběru TV (zamčený Drift). Schválil obě úpravy (viditelný zamčený segment +
nápověda při narážce); 6.0.6.9 nahrána na desku a změřena. Další kroky beze změny (viz 6.0.6.7).

# Pokračování — V6.0.6.8 (akční pohled)

Uživatel chtěl u Launch/Boostu dotáhnout model auta a stejný obraz na každé stránce; schválil „akční pohled“
(boční pohled + světla + proudnice). 6.0.6.8 nahrána na desku a změřena (README); na displeji ji uživatel ještě
neviděl. Další kroky beze změny (viz 6.0.6.7 níže): zpětná vazba → CANertia Studio → jádro + role → Energy → Drive.

# Pokračování — V6.0.6.7 (Performance Torus)

Koncept s uživatelem (2026-10-21, „souhlas se vším, ale dej možnost měnit“): BP = chytrý ovladač pro automotive,
sportovní EV obecně. Tři fyzické Torusy v jedné krabičce (USB-C 5 V/3 A, vypínač, interní CAN, výstupy CAN H/L
a USB-C pro CANable): Energy (Regen, Power, Battery; při nabíjení Charge Limit/Current; long press One-Pedal),
Drive uprostřed (Drive Mode Comfort/Sport/Track/My jako master, Ride Height Auto/Low/Lift, Limiter; long press Lift),
Performance = tento firmware. Gesta jen otočení/klik/long press, žádný double press.
6.0.6.7 nahrána na desku (COM4) a změřena (README). Uživatel ji na displeji ještě neviděl.
Další kroky (pořadí dohodnuté): 1) zpětná vazba uživatele k Performance; 2) CANertia Studio: panel vozidla
(plyn/brzda/Park-Drive-Nabíjení, jednoduchá fyzika, posílá 0x100), scénář s popisky pro komisi, výpadek vozidla,
čitelný CAN log, kontrolky Torusů, karty stavu místo pixelového náhledu halo.py, nové názvy TV/pořadí RWS;
3) firmware = jádro + role (volba při kompilaci, 3× .bin); 4) Energy (nejdřív náhled designu); 5) Drive; 6) spolupráce.

# Pokračování — V6.0.6.6 (opravy po 6.0.6.5)

Uživatel: 6.0.6.5 „TOP smooth“ = nový baseline plynulosti pro UI dalších dvou budíků (stejný design, barvy, jazyk;
jiné funkce). Názvy: UI CANertia, PC appka CANertia Studio, budík Torus, všechny tři Tori. Opravy 6.0.6.6 ověřil
uživatel na desce (tečka pryč, Titan OK). Další kroky: přechod stránek v menu (~56 fps), plnění loga ve startupu.

# Pokračování — V6.0.6.5 (výkon Halo 2, krok 5)

Uživatel k 6.0.6.4: „startup a menu 90 % there, zoom a začátek po logu (kuličky dělají kruh) furt laggy“, povolil
nahrávat přímo do ESP. 6.0.6.5 nahrána na desku (COM4) a změřena: viz README. První SCT verze s quad příkazy
nekreslila nic (uživatel: „nic se nevykresluje“) → test čtverců ukázal, že panel bere jen příkazy po jedné lince;
opraveno (QSPI_SPLIT 1), znovu změřeno, displej kreslí.
Další kroky: přechod stránek v menu (crossfade ~22 ms: auto + odraz + horní pohled, ~99k px), startup plnění loga
(pás 264–280 ~8,5 ms), cena ovladače SCT (~60 µs CPU na obdélník; vlastní registrový DMA řetězec by ji odstranil).


Serial 6.0.6.3: UI 24–39 fps; upd ~0,8 ms, rest 1,7–3,4 ms, paint 8–20 ms při work 8–25 tisíc (~0,9 µs/krok),
QSPI 3–7 ms. Uživatel: začátek a konec zoomu dobré, průběh zoomu a kuličky prstence po startupu nejhorší,
menu „lehce laggy, not up to my standard“. 6.0.6.4: kreslení v interní RAM + dělení-free míchání (viz README).
Další kroky dle Serialu: zvětšení poolu / tiling velkých obdélníků, překryv kreslení s DMA.


Serial 6.0.6.2: UI 28–42 fps, draw 10–23 ms (~0,5 µs/px), QSPI 3–9 ms (~0,2 µs/px), TE 10–15 ms (snímky
těsně nad 16,6 ms → 30 fps), max 70–80 ms při vstupu do výběru (puls prstence + zoom + segmenty naráz).
Uživatel: „velký pokrok, ale pořád trochu laggy, hlavně Aero zoom“. 6.0.6.3: viz README; čeká se na Serial
s rozpadem upd/rest/paint/work — podle něj další krok (rychlejší blend bez dělení 255, překryv kreslení s DMA,
dlaždice v interní RAM, nebo menší puls prstence).


Serial 6.0.6.1 od uživatele: startup 43–59 fps; UI 20–45 fps, draw 10–66 ms, QSPI 6–50 ms,
px 21–165k/snímek, tj. ~0,4 µs kreslení + ~0,3 µs QSPI na pixel. Uživatel: „lepší, ale furt dost laggy,
dělej co můžeš ať je to 60 fps“ (šel spát; nic se nenahrálo).
6.0.6.2: FAST_QSPI (DMA), řádkové vyplnění křídla, poškození podél čar, levnější kreslení — viz README.
Na hostu vše PASS (čisté rendery, AA, stall, Python, GUI); fyzicky NEOVĚŘENO.
Po nahrání: první řádky Serialu (FAST_QSPI unavailable? = selhala inicializace) a řádky `ui:`.
Kdyby byl displej rozbitý (šum, posunuté řádky): `#define FAST_QSPI 0` v .ino a nahrát znovu → zpět
přenos knihovny s ostatními zrychleními. Další kroky podle QSPI/draw: překryv kreslení s přenosem,
rychlejší blend (bez dělení 255), menší pásy prstence.

## Stav 6.0.6.1

Uživatel nahrál 6.0.6: „extrémně laggy, startup i UI nepoužitelné“. Serial 6.0.6:
infinity+laser 35,6 fps draw 10,5 TE 7,4 QSPI 3,6 max 61 ms; close 38 fps draw 9,4 TE 9,3;
Titan 57 fps draw 3,7 TE 6,5. Příčiny a opravy v 6.0.6.1:
- TE takt bral nejhorší snímek jako odhad → snímky čekaly na další TE. Nyní průměr, start hned.
- Plný prstenec testoval každý pixel každého obdélníku → jen řádkové úseky mezikruží.
- Logo procházelo celé řádky (zavírání 9,4 ms) → binární hledání úseku v obdélníku.
- Stuha se přepočítávala pro každý obdélník → jednou za snímek. Odraz po pásech 4 řádků.
- Změna fáze startupu překreslovala 386×214 → jen co stará fáze ukazovala (max 82k → 46k px).
- Vstup do UI překreslil ~140k px → UI převezme obraz Titanu, kreslí se jen skleněný oblouk.
Další krok: uživatel nahraje 6.0.6.1 a pošle řádky startup + `ui:` (listování stránek,
změny režimů, Aero zoom). Podle nich zmenšovat px/snímek (prstenec ~55–73k, přechod stránek ~120k).

## Stav 6.0.6

Uživatel schválil Halo 2 a startup 10 („good“) a požádal o nasazení do kódu,
maximální plynulost a stejný styl i animace v Python aplikaci (bez EXE).

Hotovo:
- Firmware: HaloStartup.h, HaloUi.h, HaloDraw.h, HaloAssets.h (obrysy loga),
  nový DdeRenderer.h (TE takt, přesné škody), DdeNavigation.h (250 ms potvrzení),
  zjednodušený DynamicsModel.h (bez lokální telemetrické ilustrace). Arduino compile OK.
- Host test: každý odeslaný snímek == čistý render, sudá okna CO5300, jeden snímek
  na TE hranu, učení periody TE, výběr/odeslání/zrušení/auto-zavření, vzdálené příkazy.
- PC: Vehicle_ECU_Simulator/halo.py (port startupu a UI), nová paleta, testy OK.

Musí se ověřit fyzicky po nahrání (zatím NENAHRÁNO — čeká na pokyn uživatele):
- FPS na desce (Serial report), hlavně přechod stránek (~70–120k px/snímek)
  a Aero zoom (až ~200k px/snímek). Pomalé kreslení přejde na rovnoměrných 30 fps.
- Enkodér: 250 ms potvrzení, okraje, dlouhý stisk; obousměrný CAN s PC.

Další možné kroky: podle naměřených FPS zmenšit škody při zoomu (dělení cest),
3D až uživatel dodá model (zaparkováno). Jeden .ino zůstává samostatný požadavek.
Starší revize/ZIPy neměnit. V5.6.1 zde není. Boční displeje nejsou implementované.
