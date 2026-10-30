# Pokračování — V6.0.5.2

Aktuální oprava: fotografie uživatele ukázaly roztrhané a posunuté kresby.
dirty() nyní rozšiřuje oblasti na sudé hranice CO5300 před restore/draw/pack.
Před opravou nový test odmítl už první startup přenos 50,130,368,155.
Test kontroluje každý přenos; navíc 22 vzorových přenosů ověřuje stride,
okraje a zachování okolí. Původní host test nemodeloval omezení panelu.
Fyzické potvrzení opravy a měření >=30 FPS stále vyžaduje skutečný dial.
CAN, model, assets a Python zůstávají beze změny; nic nebylo flashováno.

Aktuální práce je choreografie samotného prostředního DDE. Boční displeje jsou
budoucí práce; nepřidávat nyní čekání na jejich start ani nové CAN zprávy.

Uživatel schválil mikroanimace jako provázanou posloupnost, nový minimalistický
top-view a jeden hlavní Arduino tab. Podrobný výsledek: CHOREOGRAPHY_CZ.md.
Logo musí být skutečně rozbalené/sbalené z/do kuličky, hold nyní 1 s.
Celý startup 3710 ms. Výchozí Active Aero / Downforce.
Oprava podle posledního upřesnění: logo se odkrývá po svislých pruzích ze
středu ven a zakrývá od okrajů dovnitř, přesně jako princip původního Climate.
Velikost a poloha písmen zůstává pevná; žádné zvětšování/zmenšování.

Dial je vždy samostatný. PC je pouze druhé ovládání a mirror režimů.
Model, volby i animace fungují bez ECU. V6.0.5.2 CAN zůstává stejný jako V6.0.4.
PC browse/edit a fáze animace nejsou sdílené, pouze režimy a stránka.

OJM_DDE_Dynamics.ino obsahuje hardware setup a loop. Headers a assets jsou
v OJM_DDE_Dynamics/src. build_opt.h musí zůstat v kořeni sketch.
Nový top-view originál je assets/source/titan_top_minimal.png.
tools/build_artistic_assets.py připravuje malý asset a pevné logo.
LogoFolds.h je odstraněný. PC appka a EXE jsou beze změny V6.0.5.

DdeRenderer.h: clipped dirty regions, rail morph 3/4, latest-wins rail spring,
RWS→TV společný obrys, pulzy uvnitř siluety a odložená proudnice.
DynamicsModel.h: tlumené mechanické pohyby; LocalPresentation.h: cíle režimů.
Žádné změny transportu a protokolu proti V6.0.4.

Ověřování cíleně. Host renderer WASI, finální Arduino CLI, Python unittest
a GUI. Tk/Arduino/PyInstaller zde potřebují místní knihovny mimo sandbox.
VALIDATION.md uvádí výsledky a omezení; nic nebylo automaticky flashováno.

Další krok na zařízení: startup a jeho přeskočení, dorazy/rychlé točení,
RWS/TV/Aero bez ECU, obousměrné změny s PC, odpojení PC, měření FPS a tearing.
Všechny starší revize a ZIPy zachovat; V5.6.1 není ve workspace.
