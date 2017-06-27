@echo off

set DebugCompilerFlags=/MTd /Od
set ReleaseCompilerFlags=/MT /O2 /Oi /fp:fast
set CompilerConfigFlags=%ReleaseCompilerFlags%

set CommonCompilerFlags=%CompilerConfigFlags% /nologo /Gm- /GR- /EHa- /EHsc- /WX /W4 /wd4201 /wd4100 /wd4189 /wd4505 /wd4996 /Zi /FC

set CommonLinkerFlags=/incremental:no /opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

del *.pdb > NUL 2> NUL
echo WAITING FOR PDB > lock.tmp
del lock.tmp
cl %CommonCompilerFlags% ..\src\main.cpp /Fmmain.map /link %CommonLinkerFlags% 

xcopy main.exe ..\bin\ /Y /R /H

popd
