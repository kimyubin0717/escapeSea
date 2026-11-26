@echo off
setlocal
set "ABYSS_CITY_EDITOR=C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%ABYSS_CITY_EDITOR%" exit /b 1
start "Abyssal City Streetscape Editor" /belownormal /affinity F "%ABYSS_CITY_EDITOR%" "%~dp0mama.uproject" -corelimit=3 -EnablePlugins=PythonScriptPlugin -AbyssEditorTopView
