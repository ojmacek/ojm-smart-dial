# V6.0.6 — Nekonečno → laser

| Čas od startu | Děj |
|---|---|
| 0–1034 ms | Dvě kuličky se stuhou vjedou z levého a pravého okraje a každá nakreslí jednu půlku ∞ stejnou konstantní rychlostí 0,5 px/ms; uprostřed se míjí v 0,65 s. |
| 1034–1254 ms | Kulička A odjede pravým koncem s malým zábleskem; čára ∞ se do 1,41 s vytratí. |
| 1034–2750 ms | Kulička B zrychlí přes spojku do O na rychlost laseru 1,72 px/ms a obkreslí všech 11 obrysů loga s jiskrami (na konci jemný dojezd). |
| 2690–3540 ms | Celé logo se najednou rozsvítí do bílé; obrysy laseru se rozplynou. |
| 3690–4590 ms | Hold celého loga (jeden přenos, pak nic). |
| 4590–5390 ms | Logo se zavře do bílé tečky, ta vystoupá a zmizí. |
| 5390–6490 ms | Titan: střecha, zbytek obrysu s tečkami, křídlo vyjede na úhel zvoleného Aero režimu. |
| od 6490 ms | UI Halo 2: prstenec se rozběhne od horní značky (520 ms), texty najedou (320 ms). |

Poslední snímek Titanu je pixelově shodný s prvním snímkem UI (render_test).
Stuha, hvězda a jiskry jsou bílé; laserová linka má jemný odstín #DCE5FF,
logo je čistě bílé. Vstup startup přeskočí a hned se zpracuje; CAN běží celou dobu.
Geometrie a časová tabulka (krok 0,25 ms) se spočítají jednou v setup(); každá fáze
je čistá funkce času, takže jde překreslit po libovolných obdélnících.
Rostoucí ∞ a laser poškozují jen nově nakreslený úsek; rozsvícení loga celé logo.
