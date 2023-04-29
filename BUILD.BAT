if exist pub\nul deltree /y pub
mkdir pub

nmake clean
mkdir pub\adlib
nmake OPL_ON_LPT=NO OPL=2 DRV_BIN=pub\adlib

nmake clean
mkdir pub\lpt3bc
nmake OPL_ON_LPT=YES OPL=2 DEF_PORT=0x3BC DRV_BIN=pub\lpt3bc

nmake clean
mkdir pub\lpt378
nmake OPL_ON_LPT=YES OPL=2 DEF_PORT=0x378 DRV_BIN=pub\lpt378

nmake clean
mkdir pub\lpt278
nmake OPL_ON_LPT=YES OPL=2 DEF_PORT=0x278 DRV_BIN=pub\lpt278

nmake clean
deltree /y bin
