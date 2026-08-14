# FIAT-HELL — Návod pre prevádzkovateľa ATM

Tento návod ťa prevedie od prázdnej dosky po funkčný Bitcoin Lightning ATM
vyplácajúci cez **Flash** (flashapp.me). Podporované dosky:

| Doska | Čip | Poznámka |
|---|---|---|
| ESP32-8048S050 (Sunton) | ESP32-S3 | 800×480 displej, OTA aktualizácie |
| WT32-SC01 | ESP32 | menší displej, aktualizácie len cez flasher |

Web flasher: **https://webiumsk.github.io/FIAT-HELL/** — alebo offline balík
`fiat-hell-flasher-offline.zip` z [releasov](https://github.com/webiumsk/FIAT-HELL/releases)
(rozbaľ a spusti `start-windows.bat` / `start-linux-mac.sh`).

---

## 1. Flash účet a float

1. Nainštaluj appku **Flash** ([Android](https://play.google.com/store/apps/details?id=com.lnflash),
   [iOS](https://apps.apple.com/us/app/flash-bitcoin-wallet/id6711339709)) a zaregistruj sa
   svojím telefónnym číslom.
2. Vlož prostriedky do **Cash** walletu — z neho ATM vypláca.

> **Prečo Cash a nie Bitcoin wallet?** Bitcoin wallet v appke je non-custodiálny
> (kľúče sú len v tvojom telefóne), takže ATM z neho vyplácať nevie. Cash wallet
> je custodiálny a ATM ho použije cez API; pri výplate Flash konvertuje
> USD→sats automaticky. Bonus: tvoj float má stabilnú dolárovú hodnotu
> (nezáleží mu na pohybe kurzu BTC). Počítaj s tým, že Flash si pri konverzii
> účtuje vlastný kurz — nastav % poplatok ATM tak, aby ho pokryl.

## 2. Flashovanie zariadenia

1. Otvor flasher v **Chrome** alebo **Edge** (desktop).
2. **Krok 1 – Konfigurácia:** vyplň menu, hodnoty bankoviek, max. sumu, poplatok
   a heslo AP portálu (min. 8 znakov). Funding source nechaj **Flash**.
   - **WiFi:** tlačidlom „📶 Načítať siete zo zariadenia" vyberieš sieť zo
     zoznamu (funguje, keď už v zariadení je FIAT-HELL firmvér; pri prvom
     flashi zadaj názov ručne).
   - **Heslá:** štandardne sa v prehliadači nepamätajú. Buď zaškrtni
     „Zapamätať aj heslá", alebo si celý formulár ulož tlačidlom
     **💾 Uložiť profil** do súboru (obsahuje aj heslá — drž ho v bezpečí)
     a nabudúce ho len načítaš.
   - API kľúč zatiaľ nechaj prázdny — vytvoríš ho v kroku 3 priamo v ATM.
3. **Krok 2 – Flash:** vyber dosku, pripoj USB:
   - **S3:** drž `BOOT`, stlač a pusť `RESET`, pusť `BOOT`, klikni
     **⚡ Flash + nahraj konfig**.
   - **WT32:** len klikni — resetne sa sama.
   - Port smie používať len jeden program — zavri sériové monitory a duplicitné taby.

## 3. Flash API kľúč (sprievodca v ATM)

1. Zariadenie po nabootovaní pripoj na WiFi (ak si ju zadal vo flasheri, už je).
2. Otvor v prehliadači stránku zariadenia **`http://<IP-zariadenia>/flashkey`**
   — odkaz „⚡ Nemáš Flash API kľúč?" nájdeš aj v portáli pri poli API kľúč.
3. Zadaj telefónne číslo svojho Flash účtu.
4. V appke Flash si na prihlasovacej obrazovke vyžiadaj **SMS kód** —
   **kód nezadávaj do appky**, zadaj ho do sprievodcu (platí len pár minút).
5. Sprievodca vytvorí kľúč, uloží ho do zariadenia a **raz** ti ho zobrazí —
   odlož si ho (správca hesiel). Wallet ID netreba, zariadenie si ho zistí samo.

Kľúč nikdy neopúšťa zariadenie — vzniká priamo v ATM a nejde cez žiadnu
webstránku ani tretiu stranu.

## 4. Prvé spustenie a test

1. Hlavná obrazovka ukáže kurz BTC a zostatok Cash walletu prepočítaný do
   tvojej meny. `OFFLINE` = problém s WiFi; `0` = prázdny Cash wallet alebo
   zlý kľúč (pozri sériový monitor / sekciu Problémy).
2. Sprav skúšobnú výplatu najmenšou bankovkou do vlastnej peňaženky
   (inej než Flash appka).
3. Porovnaj strhnutú sumu z Cash walletu s vyplatenými sats — over, že tvoj
   % poplatok pokrýva konverzný kurz Flashu.

## 5. Zmeny nastavení

- **AP portál:** podrž pri štarte `BOOT` (S3) alebo ťukni na logo, pripoj sa
  na WiFi **LN ATM-xxxx**, otvor `http://192.168.4.1`
  (login `admin` + tvoje heslo portálu).
- **Bez portálu:** vo flasheri zmeň hodnoty a klikni **⬆ Iba konfig**
  (netreba flashovať firmvér).
- **Nový firmvér:** S3 vie OTA cez portál; WT32 sa aktualizuje flasherom
  (výber verzie z releasov priamo vo flasheri).

## 6. Bezpečnosť

- **Čo kam ide:** flasher je statická stránka — všetky údaje ostávajú
  v prehliadači a idú len cez USB do zariadenia. Zo siete sa načíta iba
  zoznam verzií z GitHub API a pri výbere staršej verzie firmvéru aj jej
  .bin súbory z GitHub releasov (lokálna „Najnovšia" verzia sa nesťahuje).
  API kľúč vzniká v zariadení.
- **Profil súbor** obsahuje heslá a kľúč v čitateľnej podobe — ukladaj na
  šifrovaný disk alebo do správcu hesiel.
- **Únik/rotácia kľúča:** vytvor nový kľúč cez `/flashkey` (starý ostáva
  platný!) a starý zneplatni: prihlás sa rovnakým spôsobom (SMS kód) a zavolaj
  `apiKeyRevoke` — podrobný postup s presnými príkazmi je v
  [Flash API dokumentácii](https://docs.flashapp.me/guides/api-keys).
  Pri podozrení na únik to sprav okamžite — kľúč vie míňať tvoj Cash wallet.

## 7. Problémy

| Príznak | Príčina / riešenie |
|---|---|
| Flasher visí na „Synchronizujem s bootloaderom" | Port drží iný program (sériový monitor, druhý tab) — zavri ho a skús znova; na S3 zopakuj BOOT+RESET tanec |
| Zariadenie po flashi nenabootuje | Skontroluj, či bola vybraná správna doska (rôzne flash adresy!) |
| Balance `OFFLINE` | WiFi nefunguje — skontroluj sieť/heslo v portáli |
| Balance `0`, v appke peniaze sú | Prostriedky musia byť v **Cash** wallete (nie Bitcoin wallete); over aj platnosť API kľúča |
| `PAYMENT FAILED` na displeji | Nedostatočný Cash zostatok (suma + poplatok), alebo zrušený kľúč — pozri sériový monitor (115200 baud), riadok `Payment error message` |
| SMS kód „nesprávny alebo expirovaný" | Kód platí pár minút a je jednorazový — vyžiadaj v appke nový |
