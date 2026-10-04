# Stav zdrojů pro GitHub — V6.0.5.3

Jde o rozpracovanou nejnovější revizi, ne hotové vydání.
Poslední ověřený release je V6.0.5.2. Předchozí adresáře a archivy nebyly změněny.
V5.6.1 zůstává stabilní baseline; v tomto workspace není dostupná.

## Připravené změny

- Klik browse/edit přímo přepíná indikátor bez postupného stmívání karoserie.
- RWS pořadí na displeji a PC: AUTO, STABLE, AGILE, OFF.
- Zachované CAN hodnoty: AUTO=3, STABLE=1, AGILE=2, OFF=0.
- Doplněné kontroly pořadí a okamžité reakce do host a GUI testů.
- Startup včetně rychlosti teček je zachovaný.

## Zbývá před vydáním

- Spustit a případně opravit doplněné testy rendereru i PC aplikace.
- Dokončit uživatelem požadovaný převod vlastního firmwaru do jednoho .ino.
- Aktualizovat návazné nástroje a dokumentaci na konečnou strukturu zdrojů.
- Finální Arduino build, export firmwaru, nový Python EXE a kontrola balíku.
- Na fyzickém dialu ověřit reakci na klik, režimy, CAN a FPS.

README, AGENTS, CAN_PROTOCOL, CHOREOGRAPHY a VALIDATION obsahují také popis
převzatého základu V6.0.5.2. Jeho výsledky nejsou ověřením nových změn.
Do Gitu patří zdroje, původní assets, licence a testy. Lokální prostředí,
cache, sestavené EXE/BIN a ZIP balíky se ignorují.
