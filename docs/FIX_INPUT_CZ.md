# V6.0.5.4 — odezva enkodéru a kliknutí

Firmware: firmware/OJM_DDE_V6_0_5_4_merged.bin, 8 MB, offset 0x0.
Arduino: OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino a přiložená složka src.
PC: Vehicle_ECU_Simulator/dist/OJM_DDE_Virtual_ECU.exe.

## Co se změnilo

Původní celočíselný přepočet vyžadoval všechny čtyři hrany kroku. Například
série raw 3 → -1 → 3 → -1 nevyvolala žádný krok, protože zbytek stále
osciloval mezi +3 a -1. Nový přepočet používá symetrickou hysterezi:
přijme krok už při třetí hraně, ale střed posune o celé čtyři impulzy.
Poslední hrana pak nevytvoří další krok a návrat o jednu hranu nepřepne
volbu zpět. Hardware nadále počítá plnou kvadraturu se stejným filtrem.
Nemaže se zbytek při každé změně směru ani při stisku. Velké dávky se
nezabalí v int16_t; zbytek zůstane pro následující průchod.

Když se ve stejném průchodu načte krátký klik a otočení, nejdřív se přepne
browse/edit a pak se aplikuje otočení. První otočení po kliknutí tak mění
režim místo nechtěného přechodu na jinou stránku. Tlačítko má nadále
24ms debounce, krátký klik při puštění a dlouhý stisk po 700 ms.

Převzaté a nyní otestované změny z rozpracované V6.0.5.3:

- Okamžitý vstup/výstup z výběru: indikátor se přepne v prvním snímku.
  Odstraněná animace stmívání celé karoserie při kliknutí.
- RWS: AUTO → STABLE → AGILE → OFF na dialu, PC ovladači i indikátoru.
  CAN hodnoty stále AUTO=3, STABLE=1, AGILE=2, OFF=0.

Startup, rychlost kuliček, CAN, piny a oprava zarovnání CO5300 zůstávají.
Převod do jediného .ino je samostatný nedokončený požadavek; tato oprava
zachovává dosavadní strukturu, aby šla rovnou nahrát jako celý sketch.

## Ověření

Test používá skutečné C++ BaselineInput.h s náhradou počítadla a GPIO:
neúplná poslední hrana, obrácení směru, krátké návraty, 2 000 plných kroků,
velké dávky a debouncovaný klik současně s otočením. Renderer porovnal
3 461 snímků s čistým renderem, zachoval 101 kontrol loga a 512 fází TV.
35 Python testů prošlo. Zbývající výsledky sestavení a GUI jsou ve VALIDATION.md.

Test ověřuje změnu chování softwaru. Bez měření fyzického enkodéru nemůže
prokázat, že právě tato situace způsobuje všechny hlášené výpadky.
Nic nebylo automaticky nahráno do desky.
