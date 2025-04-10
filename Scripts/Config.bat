@echo off

@echo Configuring paths...

rem System paths
set EngineDiskRoot=C:\

rem Unreal Engine path
set EnginePath=%EngineDiskRoot%Dev\Unreal Engine\UE_5.5

rem This thing might be located anywhere so can't be parametrized
set VersionSelector=F:\Epic Games Launcher\Epic Games\Launcher\Engine\Binaries\Win64\UnrealVersionSelector.exe

rem Editor paths
set EditorPath=%EnginePath%\Engine\Binaries\Win64\UnrealEditor.exe
set RunUATPath=%EnginePath%\Engine\Build\BatchFiles\RunUAT.bat

rem Project params
set ProjectRoot=%~dp0..\
set ProjectName=ChillFarmGame
set ProjectFile=%ProjectName%.uproject
set ProjectPath=%ProjectRoot%%ProjectFile%
set SourceCodePath=%ProjectRoot%Source

rem Cleaning params
set DirectoriesToRemove=Intermediate Saved