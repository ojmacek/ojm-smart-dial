# V6.0.7 — CANertia Studio

- Nová PC aplikace v Qt (PySide6) ve stylu webu OJM Systems: Clash Display + Sora, grafit, kobalt střídmě.
- Startup jako na Torusu, panel vozidla (0x100), simulace Drive Torusu (0x202), živý Performance, scénář obhajoby,
  CAN log, demo režim s emulací Performance Torusu. Firmware jen verze.

# V6.0.6.9 — viditelné zámky

- Zamčené volby jako šedé segmenty plné délky; narážka ukáže název a důvod („Track only“, „Above 120 km/h“).

# V6.0.6.8 — akční pohled

- Launch a Boost ukazují na každé stránce boční pohled auta (postupné prolnutí), startovní světla (Launch) a
  proudnice (Go, Boost). Otočení během Boostu pohled ukončí. Na desce ~59 fps.

# V6.0.6.7 — Performance Torus

- Situace vozidla (CAN 0x100) a Drive Mode (0x202): Auto u Aero a RWS volí polohu podle rychlosti/brzdy.
- RWS Off · Auto · Stable · Agile, TV Off · Auto · Sport · Drift; Off zamčené nad 120 km/h, Drift jen v Track.
- Long press v menu: Launch (stojí) / Boost 10 s (jede) s plněním a odpočtem prstence; stav v bajtu 5 rámce 0x201.
- Serial simulace auta pro desku; na desce ~59 fps ve všech nových stavech.

# V6.0.6.6 — opravy

- Zazoomovaný Titan měkce mizí před prstencem a segmenty (pás r 192–204 px, najíždí se zoomem).
- Hlava laseru po dokreslení posledního písmene zhasne (konec dráhy, ne přesná rovnost float součtů).
- Každý přenos pixelů je jeden segment (velká okna po řádcích); vývojový SHADOW_CHECK.

# V6.0.6.5 — výkon 5

- Kreslení na obou jádrech (i startup, velké obdélníky po pásech), přenos překrytý s kreslením (SCT, HW CS).
- Pool v interní RAM ~101k px, prstenec z tabulky, sklo jako prvek scény, 5bitové míchání, puls 30 Hz při zoomu.
- Na desce: zoom 59 fps, RWS 59, menu 56, startup plnění loga 44 fps. Vývojové Serial příkazy (B/T/R/J).

# V6.0.6.4 — výkon 4

- Kreslení poškozených obdélníků do poolu v interní RAM a přenos odtud (PSRAM jen pro přesah).
- Míchání barev bez dělení, bitově shodné. Serial `pool=`. Vzhled beze změny.

# V6.0.6.3 — výkon 3

- Řádkové úseky dlouhých čar (Aero zoom), oblouky bez atan2/sqrt v jádru, přesné boxy oblouků.
- Optimalizované pásy prstence; -O2 pro kreslicí kód; Serial rozpad upd/rest/paint/work. Vzhled beze změny.

# V6.0.6.2 — výkon 2

- FAST_QSPI: DMA přenos ze dvou interních bufferů, převod přímo z framebufferu (přepínač v .ino).
- Křídlo řádkovým vyplněním; skupiny úseků mimo obdélník se přeskakují; řádkový index obrysu; text bez dělení.
- Poškozené oblasti podél čar (auto, odraz, horní pohled, kola, segmenty, lišta, prstenec podle změny).
- Černé plochy se mažou memset; DIRTY_MAX 48 s jistým ukončením slučování. Vzhled pixelově beze změny.

# V6.0.6.1 — výkon

- TE takt: start snímku hned po předchozím, průměrný odhad kreslení (6.0.6 čekala 7–9 ms na TE).
- Levnější kreslení: prstenec jen přes mezikruží, logo jen v obdélníku, stuha jednou za snímek, odraz po pásech.
- Bez celoplošných překreslení při změně fáze startupu a při vstupu do UI.
- Serial report `ui:` každé 3 s při pohybu. Vzhled pixelově beze změny.

# V6.0.6 — Halo 2

- Nasazený schválený návrh Halo 2 (dial i PC): prstenec, značky stránek, segmenty režimů, Michroma 20/40 px.
- Nový schválený startup „Nekonečno → laser“ 6,49 s; Titan navazuje na UI bez změny pixelu.
- Aero zoom s lícujícím křídlem, RWS kola/fáze/stopy, TV hladiny s nalitím a odpichem.
- Výběr režimu: odeslání po 250 ms klidu, klik odešle, dlouhý stisk zruší, 8 s auto-zavření.
- Přesné poškozené oblasti (bez slučování do velkých oken), takt zamčený na TE, vzorkování v čase zobrazení.
- PC aplikace: Python port stejného startupu a UI (halo.py), paleta Halo 2; EXE se nestaví.
- CAN, vstup, piny, TE/QSPI, AA backend zachované. Fyzické FPS čekají na měření po nahrání.

# V6.0.5.8

- Čistě černé pozadí na dialu i PC; Title Case názvy, Michroma zachovaná.
- Předpočítaná pevná stopa nekonečna s přesným pořadím AA překryvů.
- AA optimalizace s původním fallbackem a volbou dle měření přímo na MCU.
- Celý startup557 snímků pixelově shodný s5.7, včetně časování a2,5s loga.
- Černý vstup do UI nepotřebuje přenos celého panelu.
- CAN, vstup, model, TE/QSPI zachované; fyzické FPS nové revize čekají na měření.

# V6.0.5.7

- Michroma, grafit/bílá/modrá na dialu i PC podle webové reference.
- Delta přenosy Titanu a zavření; rychlejší AA čáry a cache stuhy.
- Odstraněný 64×powf zásek na prvním snímku; nezměněný timing a 2,5 s logo.
- PC frame timer bez vynechávání každého druhého snímku; adaptivní rozestupy.
- CAN/model/vstup/TE/QSPI zachované. Fyzické FPS nové revize čekají na měření.

# V6.0.5.6

- Optimalizace startupu při zachování přesného vzhledu a všech časování.
- Podpis o 69,2 % méně přenesených pixelů, zavírání o 49,1 % méně.
- Indexy řádků a odhalování inkoustu, rychlé hotové pixely, cache taperu stuhy.
- Sqrt jen na AA hranách; 565 snímků přesně shodných s 5.5.
- Čistý referenční startup porovnávaný s panelem v každém host snímku.
- Skutečné FPS a draw/TE/QSPI časy přes neblokující jednorázový Serial report.

## V6.0.5.5

- Schválený Nekonečný podpis: bílá kulička a stuha, O → J → M.
- Přesná maska dodaného loga; M bez závěrečného doplnění ramen.
- 2,5 s celé logo s jediným přenosem, poté bílá tečka a Titan.
- Předpočítané nativní dráhy a odhalení pixelů, žádné runtime PNG/SVG ani heap.
- CAN, vstupní hystereze, běžné UI a Python EXE beze změny proti 5.4.
- 3784 kontrolovaných C++ snímků, 35 Python testů, Arduino build a audit.

## V6.0.5.4

- Symetrická hystereze enkodéru, zachovaný zbytek a velké dávky bez přetečení.
- Klik ve stejném průchodu před otočením, takže první otočení patří nové volbě.
- Dokončené V6.0.5.3: okamžitý browse/edit a RWS AUTO/STABLE/AGILE/OFF.
- Ověření skutečného input headeru, rendereru, navigace a Python ovládání.
- Startup a CAN beze změny. Převod do jediného .ino zůstává nedokončený.

## V6.0.5.2

- Oprava dílčích přenosů CO5300: sudé souřadnice a rozměry před kreslením,
  sloučením oblastí a zabalením pixelů do přenosového bufferu.
- Host kontroluje každý přenos a testuje 22 vzorových oken včetně okrajů.
- Startup, animace, CAN a Python beze změny proti V6.0.5.1.

## V6.0.5.1

- Oprava loga: pevná velikost, odkrývání od středu a zavírání ke středu jako
  v Climate. Odstraněné škálované kroky LogoFolds.h. Hold nadále 1 s.
- Startup 3,71 s. PC aplikace a funkční stránky zůstávají shodné s V6.0.5.

## Základ převzatý z V6.0.5

- Implementovaná choreografie prostředního dialu; samostatný provoz zachován.
- Magnetické setkání teček, předpočítané rozbalení a sbalení loga, hold 1 s.
- Startup 3,5 s, kreslení Titanu a mechanické usazení integrovaného křídla.
- Plné názvy funkcí a nový minimalistický top-view ze zdroje uživatele.
- Indikátor 3 stránky ↔ 4 režimy, červený bod, směrová reakce, dorazy,
  potvrzení, latest-wins a jemné zvýraznění editace.
- RWS: pružné mechanické přechody, vnější oblouky, klidná nula v Auto.
- Vectoring: žádné trvalé čáry přes obrys; emisní body, kapsle, reakce kol,
  slabší druhý pulz Max, plynulé změny síly.
- Aero: pevný pivot, odložená řídká proudnice podle polohy křídla.
- Aktualizovaný Python náhled; CAN protokol a worker beze změny proti V6.0.4.
- Hlavní Arduino tab, pomocné headers v src/, grafika předpočítaná offline.
- Test čistého překreslení, geometrie pulzů, rail morph/doraz/retarget.
