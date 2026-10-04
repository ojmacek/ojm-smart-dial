# DDE V6.0.5.2 — choreografie prostředního displeje

Implementace navazuje na V6.0.4, zůstává nezávislá na PC/ECU. Boční displeje
nejsou součástí této revize. Žádné čekání na ostatní panely ani nové CAN ID.

## Startup 3,71 s

| Čas od začátku | Děj |
|---|---|
| 0–470 ms | Magnetické setkání bílé a červené tečky, zpomalené proti V6.0.4. |
| 470–770 ms | Pevné logo se odkrývá od středu ven po svislých pruzích. |
| 770–1770 ms | Celé logo 224 px drží ve středu přesně jednu sekundu. |
| 1770–2050 ms | Okraje se zavírají ke středu a zbude červená kulička. |
| 2050–2378 ms | Bod vystoupá; rychlost odpovídá předchozímu návrhu. |
| 2378–2508 ms | Bod přejde k horní linii střechy. |
| od 2478 ms | Postupné tahy Titanu, rozfázování a měkké dokončení. |
| 3200–3660 ms | Integrované křídlo vyjede, velmi lehce přesáhne a dosedne. |
| 3710 ms | Tatáž geometrie pokračuje v Active Aero / Downforce; objeví se popisky. |

Písmena nikdy nemění velikost ani polohu. Jde o princip strip reveal/close
z původního dodaného Climate kódu run_logo_formation, s neblokujícím
vykreslením v aktuální smyčce. Předchozí chybné škálování loga bylo odstraněno.

Vstup startup přeskočí a ihned se zpracuje. CAN se obsluhuje i během startupu.

## Ovládání a vizuální hierarchie

- Otočení listuje stránkami Aero → Rear Wheel Steering → Torque Vectoring.
- Klik mění browse/edit. Tři segmenty se rozevřou do čtyř režimů a zvednou
  se o 5 px; název režimu se zvedne o 2 px a auto ztlumí na 85 %.
- Červený bod plynule míří k poslední volbě, čárka se natáhne ve směru
  otočení. Vstupy se neřadí do animované fronty.
- Konec rozsahu má krátké stlačení. Potvrzení krátce stlačí segment a složí
  indikátor zpět do tří stránek. Podržení zůstává návrat bez resetu režimů.
- Plné názvy funkcí, jasné bílé popisky, tlumenější bílá karoserie, červená
  pouze pro aktivní výběr, kola, energii a proudění.
- Přechod RWS ↔ Vectoring zachová tutéž siluetu; změní se funkční vrstva.
  Boční/vrchní pohled mají krátký náběh. Další dekorativní průjezd není potřeba.

## Funkční vrstvy

**RWS:** nový dodaný top-view. Přední kola bílá, zadní s červeným akcentem,
který při návratu do nuly dohasíná. Tlumený mechanický pohyb; Agile svižnější
a protifázový, Stable souhlasný, Auto s krátkou neutrální oblastí. Při změně
se krátce objeví malé oblouky vně kol. Oblouk a stopa tvoří jeden úsporný prvek;
žádná osa mezi nápravami.

**Vectoring:** žádné trvalé trasy. Emisní bod se otevře, vyšle krátkou kapsli,
ta zhasne uvnitř volné plochy a kolo převezme energii krátkým rozsvícením
a jemným vnějším obrysem. Normal slabší, Sport s větší asymetrií, Max s druhým
slabším pulzem. Přechody síly jsou plynulé, Off nechá energii dohasnout.
Pulzy i reakce kol jsou testované proti masce obrysu, nesmějí ji překrýt.

**Aero:** červený segment je součást karoserie a otáčí se kolem pevného bodu.
Při změně režimu se nejdřív usadí křídlo; proudnice má odklad 700 ms.
Krátká stopa projede během 900 ms jednou za 3,2 s. Low Drag odchází téměř
rovně, Downforce ji za křídlem ohne dolů, Air Brake přidá druhou větev.
Auto mění zakončení podle okamžité polohy křídla. Bez permanentního blikání.

## Výkon a struktura

Všechny assets se připravují předem. Model a renderer nepoužívají blokující
animace ani runtime dekódování PNG. Obnovují se pouze poškozené oblasti;
každý prvek je oříznut na příslušnou oblast a po zmizení se obnoví podklad.
Host test porovnává panel po dílčích přenosech s čistě vykresleným snímkem.
To dokládá správnost překreslování, nikoli dosažené FPS fyzického AMOLEDu.

Arduino: jeden hlavní .ino s setup/loop, podpůrné moduly a assets v src/.
build_opt.h v kořeni je nastavení kompilátoru. Přehled pro obhajobu:

| Modul v src/ | Úloha |
|---|---|
| DdeNavigation.h | Otočení/klik, browse/edit, směr a doraz. |
| LocalPresentation.h | Samostatné ilustrační cíle jednotlivých režimů. |
| DynamicsModel.h | Aplikace režimů, mechanické dosednutí, plynulé zatížení. |
| DdeRenderer.h | Startup, indikátor, vrstvy a přenos změněných oblastí. |
| CanTransport.h / DdeProtocol.h | Původní obousměrné režimy, ACK a heartbeat. |
| ArtTop.h / ArtLogo.h / ostatní assets | Předpočítané kresby a fonty. |

Python je bonusové ovládání a místní náhled stejné logiky. Fáze animací a
soukromý browse/edit stav dialu se CANem nesynchronizují. Firmware lze používat
plnohodnotně bez Python aplikace.
