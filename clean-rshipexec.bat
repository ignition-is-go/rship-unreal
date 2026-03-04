@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%\.." >nul

set "PLUGIN_DIR=Plugins\RshipExec"
set "TARGET_0=Intermediate"
set "TARGET_3=Binaries"
set "TARGET_1=%PLUGIN_DIR%\Intermediate"
set "TARGET_2=%PLUGIN_DIR%\Binaries"

echo This will delete only:
echo   %TARGET_0%
echo   %TARGET_1%
echo   %TARGET_2%
echo   %TARGET_3%
echo.

for %%D in ("%TARGET_0%" "%TARGET_1%" "%TARGET_2%" "%TARGET_3%") do (
    if exist "%%~D" (
        echo Removing %%~D
        rmdir /s /q "%%~D"
    ) else (
        echo Skipping %%~D (not found)
    )
)

echo Done.
popd >nul
exit /b 0
