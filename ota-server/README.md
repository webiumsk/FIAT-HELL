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

## Cloudflare

Ak je DNS na Cloudflare:
- Nastav SSL/TLS na "Flexible" – Cloudflare komunikuje s origin cez HTTP (port 80)
- Alebo "Full" – origin musí mať platný certifikát

Ak ESP32 hlási "connection refused", skontroluj:
- Je fw.lnpay.eu dostupný z siete, kde je ATM? (`curl -v http://fw.lnpay.eu/_catalog`)
- Cloudflare môže blokovať niektoré požiadavky (User-Agent, geo)
