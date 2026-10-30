# CANertia Studio (V6.0.7) — vzhled PC aplikace

| Role | Barva |
|---|---|
| Pozadí (grafit) | #14171C |
| Plochy (karty) | #1B2028, zvýšené #212731, studny (volby, log) #161A20 |
| Hranice | #2A3039, při najetí #3A424D |
| Text / vedlejší / terciární / vypnuto | #F3F1EB / #B4BDC9 / #7F8996 / #59616D |
| Akcent (kobalt) | #0038FF, výplň zvolené volby #18244C, malý text #7D95FF |

Nadpisy Clash Display (Medium), text Sora, „displeje“ Torusů Michroma ve svých barvách (níže). Rádius karet 12 px,
ovládacích prvků 8 px; stín jen vrstvené průhledné obdélníky; přechody 140–360 ms.

# Vzhled V6.0.6 — Halo 2

| Role | Barva | RGB565 |
|---|---|---|
| Pozadí | #000000 | 0x0000 |
| Text, obrys auta | #F2F1EC | 0xF79D |
| Funkční modrá | #2E5BFF | 0x2ADF |
| Záře | #003BFF | 0x01DF |
| Šedá (Off) | #7A808A | 0x7C11 |
| Prstenec menu / výběr | #1A1F27 / #1739B8 | 0x18E4 / 0x11D7 |
| Puls prstence | #7F9BFF | 0x7CDF |
| Značky stránek / v selektoru | #262C35 / #4A525E | 0x2166 / 0x4A8B |
| Segment / segment Off | #17275C / #262A31 | 0x112B / 0x2146 |
| Název v selektoru | #5E6672 | 0x5B2E |
| TV kolo / kolo Off | #343A44 / #2A2E35 | 0x31C8 / 0x2966 |
| Startup bílá / laser / jiskra | #F6F6F6 / #DCE5FF / #CFE0FF | 0xF7BE / 0xDF3F / 0xCF1F |

Michroma Regular (assets/fonts, OFL): název stránky 20 px, režim 40 px; ASCII RLE
glyphy generuje tools/build_theme.py. Všechny texty se vejdou do 330 px.
PC používá stejné barvy (theme.py, halo.py) a přiložený TTF jen pro svůj proces.
Na dialu se neškáluje ani nedekóduje TTF/PNG; PC kreslí Pillow s 3× supersamplingem.
