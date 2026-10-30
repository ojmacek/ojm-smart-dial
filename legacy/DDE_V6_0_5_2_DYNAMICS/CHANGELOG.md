# V6.0.5.2

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
