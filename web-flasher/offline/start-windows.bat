@echo off
rem FIAT-HELL Web Flasher - offline spustenie (Windows)
rem Web Serial vyzaduje https alebo localhost, preto sa stranka servuje lokalne.
cd /d "%~dp0"
start "" http://localhost:8123
where py >nul 2>nul && (py -3 -m http.server 8123 & goto :eof)
where python >nul 2>nul && (python -m http.server 8123 & goto :eof)
echo Python nie je nainstalovany - stiahni ho z https://www.python.org/downloads/
echo (pri instalacii zaskrtni "Add python.exe to PATH") a spusti tento subor znova.
pause
