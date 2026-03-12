# NV10USB+ Bill Acceptor – zapojenie

## Úvod

NV10USB+ používa **2 dátové vodiče** na pincoch **1** a **5** konektora. FIAT-HELL používa UART 300 baud, 8N2 (SERIAL_8N2).

## Prepojenie dátových vodičov

**Sunton ESP32-8048S050 (ESP32-S3):** GPIO 32/33 spôsobujú boot loop – použite **P3 header** (GPIO 17, 18):

| NV10USB+ Pin | Signál | ESP32-8048S050 P3 | Poznámka |
|--------------|--------|-------------------|----------|
| **1** | Tx (NV10 vysiela) | **GPIO 18** (Rx) | NV10 Pin 1 → P3 Rx (GPIO18) |
| **5** | Rx (NV10 prijíma) | **GPIO 17** (Tx) | NV10 Pin 5 → P3 Tx (GPIO17) |

Pravidlo: **Tx jednej strany ide do Rx druhej strany.**

## Napájanie NV10USB+

| NV10USB+ Pin | Pripojenie |
|--------------|------------|
| **15** | 12V DC+ (externý zdroj) |
| **16** | GND (spoločná s ESP32) |

Zapoj ESP32 GND aj na spoločný GND s bill acceptorom.

## Kde zapojiť na Sunton ESP32-8048S050

**Header P3 (4 piny):** IO20, IO19, IO18, IO17, +3.3V, GND

- **NV10 Pin 1 (Tx)** → **GPIO 18** (P3 – ESP prijíma)
- **NV10 Pin 5 (Rx)** → **GPIO 17** (P3 – ESP vysiela)
- **GND** → spoločná s NV10 Pin 16

## Aktivácia v kóde

V `src/main.cpp` zmeň:

```cpp
#define BILL_ACCEPTOR_ENABLED 1   // z 0 na 1
```

## Správanie

- **Po štarte / main screen:** Posiela sa 185 (disable); na niektorom hardvéri to nefunguje, preto sa 185 posiela periodicky.
- **Mena z bankovky:** Prvá vložená bankovka na main screene určí menu (EUR/CZK/…). Ďalšie bankovky inej meny sa nepripočítavajú.
- **Wrong currency:** Pri inej mere ako zvolenej sa okamžite posiela byte 70 (možný reject) – v SIO móde NV10 nemusí podporovať vrátenie bankovky, potom sa bankovka aj tak vťahuje (len sa nepripočíta). Pre fyzické odmietnutie je potrebný inhibit na úrovni NV10 alebo protokol s escrow (SSP/eSSP).

## Konfigurácia kanálov

V portáli na http://fiathell.local nakonfiguruj:
- **Bill mech** – hodnoty v centoch (napr. 100, 200, 500, 1000, 2000)
- **Max amount** – maximálna suma
- **Charge** – poplatok v %
