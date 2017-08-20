@echo off

set DebugCompilerFlags=/MTd /Od
set ReleaseCompilerFlags=/MT /O2 /Oi /fp:fast
set CompilerConfigFlags=%DebugCompilerFlags%
REM /WX
set CommonCompilerFlags=%CompilerConfigFlags% /nologo /Gm- /GR- /EHa- /EHsc- /W4 /wd4201 /wd4100 /wd4189 /wd4505 /wd4996 /Zi /FC -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1

set CommonLinkerFlags=/incremental:no /opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

del *.pdb > NUL 2> NUL
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% ..\src\main.cpp /Fmmain.map /link %CommonLinkerFlags% 
del lock.tmp

cl %CommonCompilerFlags% ..\src\win32_raytracer.cpp /Fmwin32_raytracer.map /link %CommonLinkerFlags% 

xcopy main.exe ..\bin\ /Y /R /H
xcopy win32_raytracer.exe ..\bin\ /Y /R /H

popd
