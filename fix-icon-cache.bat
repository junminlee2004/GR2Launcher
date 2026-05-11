@echo off
REM === GR2launcher icon-cache fixer + verifier =============================
REM
REM If you rebuilt and the launcher still shows the old shadPS4 blue-4
REM logo, it's one of two caches. This script handles both.
REM
REM   1) Windows Explorer caches the icon of every .exe it has seen. Even
REM      after a clean rebuild with the new icon embedded, Explorer keeps
REM      showing the cached old one. Touching/renaming the .exe usually
REM      invalidates the cache. Restarting explorer.exe always does.
REM
REM   2) Qt's qt_add_resources compiles the .qrc + referenced files into
REM      a generated .cpp at configure time. If your build was incremental
REM      and didn't detect the icon changed (mtime preserved, etc.), the
REM      old icon bytes stay baked into your binary. Solution: clean build.
REM
REM Run this from the folder that contains your built GR2launcher.exe.
REM Adjust the EXE path below if it lives elsewhere.

set "EXE=GR2launcher.exe"

echo.
echo === GR2launcher icon-cache fixer =====================================
echo.

if not exist "%EXE%" (
  echo [!] %EXE% not found in the current folder.
  echo     cd into your Build folder first.
  exit /b 1
)

echo Touching %EXE% to nudge Explorer's icon cache...
copy /b "%EXE%" +,, "%EXE%" >nul

echo Killing Windows Explorer and restarting it (forces icon cache flush)...
taskkill /f /im explorer.exe >nul 2>&1
timeout /t 1 /nobreak >nul
start "" explorer.exe

echo.
echo Done. If you STILL see the old blue-4 logo:
echo.
echo   - The icon is baked into the .exe from src\shadps4.rc (-^> shadps4.ico).
echo     Clean-build to make sure the resource compiler ran:
echo         cmake --build . --target clean
echo         cmake --build .
echo.
echo   - Verify the icon embedded in the .exe:
echo         right-click GR2launcher.exe -^> Properties -^> Details.
echo     "File description" should read "GR2launcher - shadPS4 launcher for gr2fork".
echo     If it still says "shadPS4 - PlayStation 4 Emulator", your .exe is stale.
echo.
