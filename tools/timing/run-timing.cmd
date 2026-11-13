@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem
rem The timing run (B-10, docs/timing.md) on Windows: the playthrough at
rem N = 1, 2, 5 and 10, one CSV per N. Put this file and playthrough.txt
rem next to colonel-hd.exe and run it with the game folder:
rem
rem   run-timing.cmd "C:\GOG Games\The Colonel's Bequest"
rem
rem The first run (N = 1) starts with an empty analysis cache, so its CSV
rem holds the first launch; the others use the cache it made.
setlocal
if "%~1"=="" (
    echo usage: run-timing.cmd ^<game folder^>
    exit /b 2
)
set "HERE=%~dp0"
set "DATA=%HERE%timing-data"
if exist "%DATA%" rmdir /s /q "%DATA%"
mkdir "%DATA%"
for %%N in (1 2 5 10) do (
    echo Timing at N = %%N ...
    "%HERE%colonel-hd.exe" --data "%DATA%" --replay "%HERE%playthrough.txt" --seed 0 --timing "%HERE%timing-%%N.csv" --scale %%N "%~1"
    if errorlevel 1 (
        echo The run at N = %%N failed; see the console above.
        exit /b 1
    )
)
echo Done: send timing-1.csv, timing-2.csv, timing-5.csv and timing-10.csv.
endlocal
