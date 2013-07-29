CFLAGS = /Gm+ /Sp /Q /Ss+ 
OBJS = usbmountd.obj getopt.obj
LFLAGS = /NOE /PMTYPE:VIO /NOLOGO
LIBS = ./lib/LVM.LIB ./lib/USBCALLS.LIB 
all : $(OBJS)
    ilink.exe $(LFLAGS) $(OBJS) $(LIBS)

