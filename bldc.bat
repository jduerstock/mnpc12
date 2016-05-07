cl /c /Gs mnpintrf.c mnpllvl.c mnpdrvr.c mnpevent.c mnpmisc.c
masm async,async;
masm suspend,suspend;
masm fcscalc,fcscalc;
masm setpar,setpar;
masm timer,timer;
masm portio,portio;
erase mnp.lib
lib @bldlib
