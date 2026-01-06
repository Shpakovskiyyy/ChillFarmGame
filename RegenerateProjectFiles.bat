@echo off

@echo Starting project files regen...

set ScriptsDirectory=Scripts/

call %ScriptsDirectory%"Config.bat"
call %ScriptsDirectory%"CleanIntermediateFiles.bat"

call %ScriptsDirectory%"GenerateProjectFiles.bat" "%VersionSelector%" "%ProjectFile%"

@echo Finished project files regen