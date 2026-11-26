@echo off
setlocal
set "ABYSS_EDITOR=C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%ABYSS_EDITOR%" (
  echo Unreal Engine 5.7 was not found. Open mama.uproject in your Unreal installation.
  pause
  exit /b 1
)
start "ABYSS - Last Light" /belownormal "%ABYSS_EDITOR%" "%~dp0mama.uproject" -game -windowed -ResX=1600 -ResY=900 -abslog="%~dp0Saved\Logs\AbyssPlay.log"
