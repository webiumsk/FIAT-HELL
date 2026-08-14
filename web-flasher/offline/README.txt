FIAT-HELL Web Flasher - offline balik
=====================================

Toto je plna kopia web flashera vratane firmware binariek pre obe dosky
(ESP32-8048S050 aj WT32-SC01). Vsetko bezi lokalne na tvojom pocitaci -
ziadne udaje sa nikam neposielaju (jedina volitelna sietova vec je zoznam
starsich verzii z GitHubu; bez internetu funguje zabalena verzia firmware).

Spustenie:
  Windows:      dvojklik na start-windows.bat  (vyzaduje Python 3)
  Linux/macOS:  ./start-linux-mac.sh

Potom sa v prehliadaci Chrome alebo Edge otvori http://localhost:8123.
Priamy dvojklik na index.html NEFUNGUJE - Web Serial API vyzaduje
https alebo localhost, preto je potrebny lokalny server.

Navod na pouzitie najdes v zalozke "Navod" priamo vo flasheri,
alebo v repozitari: https://github.com/webiumsk/FIAT-HELL
