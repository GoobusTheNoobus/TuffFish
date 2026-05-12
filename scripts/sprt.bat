@echo off
setlocal

set "NEW=.\tuff_test.exe"
set "OLD=.\tuff.exe"

if not exist "%NEW%" (
    echo ERROR: Missing NEW engine binary: %NEW%
    pause
    exit /b 1
)

if not exist "%OLD%" (
    echo ERROR: Missing OLD engine binary: %OLD%
    pause
    exit /b 1
)

for /f %%H in ('powershell -NoProfile -Command "(Get-FileHash '%NEW%').Hash"') do set "NEW_HASH=%%H"
for /f %%H in ('powershell -NoProfile -Command "(Get-FileHash '%OLD%').Hash"') do set "OLD_HASH=%%H"

if /I "%NEW_HASH%"=="%OLD_HASH%" (
    echo ERROR: NEW and OLD binaries are identical. SPRT cannot detect Elo differences.
    echo NEW: %NEW%
    echo OLD: %OLD%
    pause
    exit /b 1
)

echo Starting SPRT...
echo NEW: %NEW%  [%NEW_HASH%]
echo OLD: %OLD%  [%OLD_HASH%]

cutechess-cli ^
-engine name=NEW cmd=%NEW% ^
-engine name=OLD cmd=%OLD% ^
-each proto=uci tc=10+0.1 ^
-games 2 ^
-rounds 50000 ^
-concurrency 8 ^
-repeat ^
-sprt elo0=0 elo1=5 alpha=0.05 beta=0.05 ^
-pgnout results.pgn

pause
