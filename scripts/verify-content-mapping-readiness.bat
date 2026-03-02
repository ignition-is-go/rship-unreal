@echo off
REM Fail-fast readiness gate for content mapping runtime material availability.
REM Usage: scripts\verify-content-mapping-readiness.bat

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set REPO_ROOT=%SCRIPT_DIR%..

set FOUND=0

set C1=%REPO_ROOT%\Plugins\RshipMapping\Content\Materials\MI_RshipContentMapping.uasset
set C2=%REPO_ROOT%\Plugins\RshipMapping\Content\Materials\M_RshipContentMapping.uasset
set C3=%REPO_ROOT%\Plugins\RshipExec\Content\Materials\MI_RshipContentMapping.uasset
set C4=%REPO_ROOT%\Plugins\RshipExec\Content\Materials\M_RshipContentMapping.uasset
set C5=%REPO_ROOT%\Content\Rship\Materials\MI_RshipContentMapping.uasset
set C6=%REPO_ROOT%\Content\Rship\Materials\M_RshipContentMapping.uasset

for %%C in ("%C1%" "%C2%" "%C3%" "%C4%" "%C5%" "%C6%") do (
    if exist %%~C set FOUND=1
)

set TOTAL_UASSETS=0
for /f %%I in ('dir /s /b "%REPO_ROOT%\*.uasset" 2^>nul ^| find /c /v ""') do set TOTAL_UASSETS=%%I

echo == Content Mapping Readiness ==
echo Repo root: %REPO_ROOT%
echo Total .uasset files: %TOTAL_UASSETS%
echo Configured ContentMappingMaterialPath lines:
findstr /s /n /i "ContentMappingMaterialPath=" "%REPO_ROOT%\Config\*.ini" 2>nul
if errorlevel 1 echo (none found in Config\)

if "%FOUND%"=="1" (
    echo PASS: content-mapping material candidates are present.
    exit /b 0
)

echo FAIL: no mapping material assets found in any runtime candidate location.
echo Image-on-surface rendering cannot succeed without a valid mapping material asset.
echo.
echo Expected at least one of:
echo   - Plugins\RshipMapping\Content\Materials\MI_RshipContentMapping.uasset
echo   - Plugins\RshipMapping\Content\Materials\M_RshipContentMapping.uasset
echo   - Plugins\RshipExec\Content\Materials\MI_RshipContentMapping.uasset
echo   - Plugins\RshipExec\Content\Materials\M_RshipContentMapping.uasset
echo   - Content\Rship\Materials\MI_RshipContentMapping.uasset
echo   - Content\Rship\Materials\M_RshipContentMapping.uasset

exit /b 2
