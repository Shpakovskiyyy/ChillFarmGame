@echo off

call 'Config.bat'

pushd %ProjectRoot%

FOR %%f in (%DirectoriesToRemove%) do (
    @echo Deleting directory %%f ...
    rmdir /s /q %%f
)

rem Unused for now
rem FOR %%f in (%filesToRemove%) do (
rem     @echo Removing file %%f ...
rem     del /q %%f
rem )

popd