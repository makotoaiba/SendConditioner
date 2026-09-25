@echo off
setlocal
rem Installs Send Conditioner for the current user (no admin rights needed).
set "DEST=%LOCALAPPDATA%\Programs\Common\VST3"
pushd "%~dp0"
if not exist "Send Conditioner.vst3" (
  echo [ERROR] "Send Conditioner.vst3" was not found next to this file.
  popd
  pause
  exit /b 1
)
if not exist "%DEST%" mkdir "%DEST%"
if exist "%DEST%\Send Conditioner.vst3" rmdir /s /q "%DEST%\Send Conditioner.vst3"
xcopy /e /i /y /q "Send Conditioner.vst3" "%DEST%\Send Conditioner.vst3" >nul
if errorlevel 1 (
  echo [ERROR] Copy failed. Close the DAW and try again.
) else (
  echo Installed to: %DEST%\Send Conditioner.vst3
  echo Rescan plug-ins in your DAW.
)
popd
pause
