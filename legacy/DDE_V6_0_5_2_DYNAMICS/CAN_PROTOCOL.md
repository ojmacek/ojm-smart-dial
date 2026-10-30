# DDE CAN v2 — samostatný Titan V6.0.5

V6.0.5 nemění protokol ani CAN worker proti V6.0.4. Jde o lokální grafickou
choreografii prostředního dialu, bez nových synchronizačních rámců pro boční
displeje. Následující specifikace V6.0.4 zůstává platná. Indikátor editace je
lokální stav; PC náhled má vlastní krátkou indikaci právě vybraného režimu.

Dial je autorita režimů a lokálních animací, PC jen druhé ovládání a zrcadlo.
Requested default: page2/Aero, rws3/Auto, tv1/Normal, aero2/Downforce.
**Aero requested nyní dovoluje 3 AIR BRAKE.** V6.0.4 firmware a appku nasazuj
společně; starší V6.0.x hodnotu3 odmítá. ID, DLC, checksum a sequence jsou stejné.
Klik na PC režim odešle také výběr příslušné stránky. Změna enkodérem publikuje
stav zpět do PC. PC offline volby se při připojení automaticky nepřehrávají.
Lokální browse/edit není přenášené a příkaz stránky ho nepřepíná.

Běžná PC appka používá CanWorker(simulation=False): vysílá příkazy0x301 a přijímá
stav0x201/ACK0x211, neprodukuje telemetrii0x401. Níže uvedený legacy telemetrický
codec je zachovaný pro regresní testy/rozšíření; přijatá data nemění animace ani
lokální režimy. Vše funguje bez přítomnosti všech těchto rámců.

Classical CAN, 500 kbit/s, standardní 11bit ID, max. 8 datových bajtů.
Extended, RTR, chybný DLC a neplatné hodnoty se ignorují. Vícebajtová čísla
jsou little-endian, signed hodnoty používají dvojkový doplněk.
`XOR` znamená XOR bajtů 0–6, nikoliv náhradu CAN CRC.

## Profily a směry

| Funkce | Stav dial → ECU | Příkaz ECU → dial | ACK dial → ECU | Telemetrie ECU → dial |
|---|---|---|---|---|
| DYNAMICS | 0x201 | 0x301 | 0x211 | 0x401 |
| DRIVE, rezervace | 0x202 | 0x302 | 0x212 | 0x402 |
| ENERGY, rezervace | 0x203 | 0x303 | 0x213 | 0x403 |

V5.6.1 používala ACK 0x202. Toto ID je podle zadání vyhrazené pro stav DRIVE,
proto nelze zachovat staré ACK ID současně s třemi profily. DDE přesouvá ACK
na 0x211 a nepřijímá staré climate příkazy 0x01–0x06. Bezpečně rozlišuje stav
pomocí značky 0xD2. Jde o explicitní migraci aplikačního protokolu: formát
32bit sequence ACK, latest-wins, časování a Classical CAN jsou zachované.
Firmware a PC DDE se nasazují společně. Současné provozování staré climate
baseline a tří DDE dialů na těchto ID není podporováno.

## Příkaz 0x301

DLC 6: `[command, value, seq0, seq1, seq2, seq3]`.
Sequence je uint32 LE, v PC počáteční náhodná hodnota pro každé připojení,
inkrement modulo 2^32. DLC 2 `[command,value]` zůstává podporován pro nové
DDE kódy; takový příkaz vyvolá stav, nemá sequence ACK.

| Kód hex | Funkce | Absolutní hodnoty |
|---|---|---|
| 10 | Aktivní stránka | 0 RWS, 1 TORQUE VECTORING, 2 ACTIVE AERO |
| 11 | RWS requested mode | 0 OFF, 1 STABLE, 2 AGILE, 3 AUTO |
| 12 | TV requested mode | 0 OFF, 1 NORMAL, 2 SPORT, 3 MAX |
| 13 | Aero requested mode | 0 AUTO, 1 LOW DRAG, 2 DOWNFORCE, 3 AIR BRAKE |

AIR BRAKE 3 je od V6.0.4 platná requested hodnota. Příkazy jsou idempotentní setpointy,
nikoliv toggles. Poslední hodnota vítězí zvlášť pro každou ze čtyř funkcí.
Při rychlém vstupu se nevykreslují mezihodnoty. Přepsaný příkaz nemusí dostat ACK.
Pořadí dávkového zpracování je podle kódu, bez FIFO záruky mezi funkcemi.

## ACK 0x211, DLC 8

`[command, value, seq0, seq1, seq2, seq3, status, XOR]`.

Status 0 = aplikováno (včetně již stejné hodnoty), 1 = známý příkaz odmítnut
kvůli hodnotě. Neznámý kód se ignoruje. Sekvence se kopíruje beze změny.
ACK se zařadí po aplikování do modelu, před vykreslením, má přednost před
stavovým vysíláním. Nečeká na konec page animace. PC potvrdí pouze shodu ID,
kódu, hodnoty, sekvence, délky a checksumu již odeslaného pending požadavku.
Samotné ACK neobnovuje ONLINE. Sekvence je korelace, nikoliv autentizace ani
trvalá ochrana proti replay; počítá se s jedním aktivním producentem příkazů.

## Stav 0x201, DLC 8

| Bajt | Obsah |
|---|---|
| 0 | 0xD2, značka DDE v2 |
| 1 | active requested page 0–2 |
| 2 | RWS requested mode 0–3 |
| 3 | TV requested mode 0–3 |
| 4 | Aero requested mode 0–3 |
| 5 | rezervované faults, samostatný prezentační model posílá nulu |
| 6 | alive 0–15 |
| 7 | XOR |

Perioda 200 ms; změnový stav může přijít mezi heartbeat rámy s minimálním
intervalem 8 ms, aniž resetuje periodický deadline. Jde o best-effort podle
dostupnosti sběrnice. Alive se zvyšuje při předání driveru, ne při prokázaném
doručení. Statistika gaps není přesný počet ztracených heartbeatů.

## Legacy telemetrie 0x401 — volitelný codec, neřídí HMI

Každý rámec: `[type, generation, P0, P1, P2, P3, P4, XOR]`.
Jedna generace obsahuje všech pět typů A0–A4; generation je uint8 modulo 256.
Požadované režimy a active page jsou v příkazech/stavu výše; zde jsou actual
data vypočítaná virtuální ECU. Úhly jsou stupně; pozice/brzda/zatížení procenta.

| Type hex | P0 | P1 | P2 | P3 | P4 |
|---|---|---|---|---|---|
| A0 | speed low | speed high | front angle low | front angle high | brake 0–100 |
| A1 | rear requested low | rear requested high | rear actual low | rear actual high | RWS actual mode 0–3 |
| A2 | FL 0–100 | FR 0–100 | RL 0–100 | RR 0–100 | yaw int8 −100…100 |
| A3 | aero requested 0–100 | aero actual 0–100 | aero actual mode 0–3 | TC mask | fault bits |
| A4 | lateral acceleration low | lateral acceleration high | L/R bias int8 −100…100 | 0 rezervováno | 0 rezervováno |

- Speed: uint16, krok 0,1 km/h, rozsah raw 0–4000 (0–400 km/h).
- Front: int16, krok 0,1°, raw −450…450 (±45°).
- Rear requested/actual: int16, krok 0,1°, raw −150…150 (±15°).
- RWS actual režimy mají stejné kódy jako requested; AUTO může ECU hlásit
  jako AGILE nebo STABLE podle aktuální části rychlostního přechodu.
- Aero actual: 0 AUTO, 1 LOW DRAG, 2 DOWNFORCE, **3 AIR BRAKE**.
- TC mask: bity 0 FL, 1 FR, 2 RL, 3 RR; horní bity nula.
- Legacy faults: bit0 RWS, bit1 Aero; horní bity nula. V6.0.4 tyto bity
  uchovává pouze v přijatém vzorku, neodvozuje ani nezobrazuje actuator fault.
- Lateral: int16, krok 0,01 g, raw −300…300 (±3 g).
- Yaw a bias jsou normalizované konceptuální indikátory, nejsou v Nm.
  Kladný bias znamená více momentu vpravo; yaw je kladný pro pravou zatáčku.
- Torque jsou nezávislé normalizované hodnoty zatížení jednotlivých kol;
  součet není vynucen na 100, např. zásah TC může jednu hodnotu snížit.

Pět pevných slotů, žádná historie. Přijímač zveřejní až kompletní generaci.
Novější generace zruší neúplnou; delta modulo 256 v rozsahu 1–127 znamená
novější, 128–255 starší. Duplicitní hotová generace se ignoruje. Po >300 ms
bez akceptovaného fragmentu se assembler resetuje a přijme libovolnou
generaci. Chybějící nebo neplatný subframe nevyvolá smíchaný vizuální stav.
Při ztrátě rámce zůstane poslední kompletní stav až do další celé generace.

Legacy simulátor umí celé pětirámcové vzorky při 10/30/60 Hz, tedy 50/150/300 CAN
rámců za sekundu plus příkazy, ACK a stav. Fronta obsahuje nejvýše jeden
vzorek. Po zdržení se generuje aktuální vzorek bez dohánění starých cyklů.
Příkazy mají ve workeru prioritu před nejvýše pěti telemetrickými sendy.
Běžná V6.0.4 aplikace tuto legacy simulaci nespouští.

## Časování a offline

PC: první vstup okamžitě způsobilý k odeslání workerem, průběžné změny
max. jednou za 35 ms na funkci, vždy i poslední hodnota. Retry po 250 ms,
maximálně 3 pokusy celkem. Novější požadavek zruší staré retry. Opakovaný
stejný pending požadavek neobnovuje retry budget. Žádný validní stav po
1,2 s → OFFLINE a zrušení pending/queued příkazů. Po reconnectu
se staré příkazy automaticky nepřehrávají; PC převezme aktuální stav dialu.

Dial: přítomnost/absence telemetrie nemění režimy ani animace. Přijatý legacy
vzorek se uchovává odděleně a lokální ilustrační model ho nepřepisuje.
CAN RX/ACK stále běží na core0; UI nečeká na CAN/Serial. Telem assembler si
zachovává své timeouty, ale nezpůsobuje NO ECU/neutrál ve vizuálu.
