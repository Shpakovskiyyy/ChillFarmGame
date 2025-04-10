@echo off

set VersionSelector=%~1
set ProjectPath=%CD%\%~2

@echo Generating project files...

"%VersionSelector%" -projectfiles "%ProjectPath%"