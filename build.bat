@echo off

set CommonCompilerFlags=/MTd /nologo /Gm- /GR- /EHa- /Od /Oi /WX /W4 /wd4201 /wd4100 /wd4189 /wd4505 /Zi /FC -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1
set CommonLinkerFlags=/incremental:no /opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST .\build mkdir .\build
pushd .\build

REM 64 bits build
del *.pdb > NUL 2> NUL
REM Optimization switches /O2 /Oi /fp:fast
echo WAITING FOR PDB > lock.tmp
del lock.tmp
cl  %CommonCompilerFlags% ..\src\main.cpp /Fmmain.map /link %CommonLinkerFlags% 
REM xcopy main.exe ..\bin\ /Y

popd
