if exist pub\nul deltree /y pub
mkdir pub

nmake clean
nmake OPL_ON_LPT=NO OPL=2
mkdir pub\adlib
move bin\*.* pub\adlib

nmake clean
nmake OPL_ON_LPT=YES OPL=2 DEF_PORT=0x3BC
mkdir pub\lpt3bc
move bin\*.* pub\lpt3bc

nmake clean
nmake OPL_ON_LPT=YES OPL=2 DEF_PORT=0x378
mkdir pub\lpt378
move bin\*.* pub\lpt378

nmake clean
nmake OPL_ON_LPT=YES OPL=2 DEF_PORT=0x278
mkdir pub\lpt278
move bin\*.* pub\lpt278

nmake clean
deltree /y bin
