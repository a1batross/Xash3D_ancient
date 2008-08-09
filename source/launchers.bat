@echo off

cd launch
cd bsplib
makefile.nmake

cd ..
cd extragen
makefile.nmake

cd ..
cd qcc
makefile.nmake

cd ..
cd sprite
makefile.nmake

cd ..
cd studio
makefile.nmake

cd ..
cd viewer
makefile.nmake

cd ..
cd xash
makefile.nmake

cd ..
cd xwad
makefile.nmake
pause