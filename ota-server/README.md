# OTA Update Server pre FIAT-HELL

PHP server pre AutoConnect OTA aktualizácie.

## Deployment na shared hosting

1. Nahraj obsah priečinka `ota-server/` do document root (napr. `public_html/` alebo subdoména `fw.lnpay.eu`).
2. Vytvor priečinok `bin/` a daj mu zápisové práva.
3. Skopíruj `firmware.bin` z buildu:
   ```text
   .pio/build/esp32-8048s050/firmware.bin  →  bin/firmware.bin
   ```
4. Pre poradové verzie môžeš premenovať: `fiat-hell-v1.2.0.bin`.
5. Podpíš image a nahraj aj podpis (rovnaký názov s príponou `.sig`):
   ```text
   python3 tools/sign_firmware.py bin/fiat-hell-v1.2.0.bin
   ```
   Súbor `fiat-hell-v1.2.0.bin.sig` musí ležať vedľa `.bin`. Bez platného podpisu zariadenie update odmietne. Názov musí obsahovať `vX.Y.Z`, ktoré nie je staršie ako bežiaci firmvér.

## Štruktúra

```text
ota-server/
├── index.php    # handler pre katalóg aj download
├── .htaccess    # Apache rewrite
├── bin/         # sem daj .bin súbory
│   └── firmware.bin
└── README.md
```

## Nastavenie v main.cpp

- `OTA_UPDATE_SERVER` = hostname (bez http://), napr. `fw.lnpay.eu`
- `OTA_UPDATE_PORT` = 80 (štandardný HTTP) alebo 443 (HTTPS)

Pre HTTPS bude treba upraviť AutoConnectUpdate (zatiaľ len HTTP).

## Poznámky

- `.bin` musí byť servovaný s hlavičkou `Content-Length` (tento PHP skript ju posiela). Chunked odpoveď z CDN/proxy by update odmietla.
- Zariadenie kontroluje verziu z názvu súboru aj z markra `FHFW:X.Y.Z` vloženého v image, takže premenovanie staršieho súboru downgrade neobíde.

## Cloudflare

Ak je DNS na Cloudflare:
- Nastav SSL/TLS na "Flexible" – Cloudflare komunikuje s origin cez HTTP (port 80)
- Alebo "Full" – origin musí mať platný certifikát

Ak ESP32 hlási "connection refused", skontroluj:
- Je fw.lnpay.eu dostupný z siete, kde je ATM? (`curl -v http://fw.lnpay.eu/_catalog`)
- Cloudflare môže blokovať niektoré požiadavky (User-Agent, geo)
