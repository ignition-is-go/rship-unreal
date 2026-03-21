@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Source plugin directory (this repo's Plugins folder)
set "SRC_ROOT=%~dp0Plugins"

rem Destination plugin directories
set "DEST_A=C:\P-0616-HRLV\ue\HRLV_Content\Plugins"
set "DEST_B=C:\P-0616-HRLV\ue\HRLV_Previs\Plugins"

rem Optional preview mode: copy-pulse-plugins.bat --whatif
set "ROBO_FLAGS=/MIR /R:2 /W:1 /NFL /NDL /NP /XJ"
if /I "%~1"=="--whatif" set "ROBO_FLAGS=!ROBO_FLAGS! /L"

if not exist "%SRC_ROOT%\" (
  echo [ERROR] Source folder not found: "%SRC_ROOT%"
  exit /b 1
)

if not exist "%DEST_A%\" mkdir "%DEST_A%"
if not exist "%DEST_B%\" mkdir "%DEST_B%"

set /a FAILURES=0
set /a COPIED=0

echo Source: "%SRC_ROOT%"
echo Target 1: "%DEST_A%"
echo Target 2: "%DEST_B%"
if /I "%~1"=="--whatif" echo Mode: WHATIF (no files copied)
echo.

for /D %%P in ("%SRC_ROOT%\*") do (
  if exist "%%~fP\*.uplugin" (
    set "PLUGIN_NAME=%%~nxP"
    echo ============================================================
    echo Copying plugin: !PLUGIN_NAME!

    robocopy "%%~fP" "%DEST_A%\!PLUGIN_NAME!" !ROBO_FLAGS!
    if errorlevel 8 (
      echo [ERROR] Robocopy failed for !PLUGIN_NAME! to "%DEST_A%"
      set /a FAILURES+=1
    )

    robocopy "%%~fP" "%DEST_B%\!PLUGIN_NAME!" !ROBO_FLAGS!
    if errorlevel 8 (
      echo [ERROR] Robocopy failed for !PLUGIN_NAME! to "%DEST_B%"
      set /a FAILURES+=1
    )

    set /a COPIED+=1
  ) else (
    echo [SKIP] Not an Unreal plugin folder: "%%~nxP"
  )
)

echo.
echo Plugins processed: %COPIED%
if %FAILURES% GTR 0 (
  echo Completed with %FAILURES% failure(s).
  exit /b 1
) else (
  echo Completed successfully.
  exit /b 0
)
