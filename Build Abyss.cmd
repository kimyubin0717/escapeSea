@echo off
setlocal
set "ABYSS_BUILD=C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat"
if not exist "%ABYSS_BUILD%" exit /b 1
if exist "%~dp0Intermediate\Build\XmlConfigCache.bin" (
  call "%ABYSS_BUILD%" mamaEditor Win64 Development "-Project=%~dp0mama.uproject" "-XmlConfigCache=%~dp0Intermediate\Build\XmlConfigCache.bin" -NoUBTMakefiles -NoHotReloadFromIDE -NoUBA -WaitMutex "-Log=%~dp0Saved\Logs\AbyssBuild.log"
) else (
  call "%ABYSS_BUILD%" mamaEditor Win64 Development "-Project=%~dp0mama.uproject" -NoHotReloadFromIDE -NoUBA -WaitMutex "-Log=%~dp0Saved\Logs\AbyssBuild.log"
)
pause
