#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS   = gtxenx_core.o gtxenx_fb.o avia.o
obj-m    = gtxenx_core.o gtxenx_fb.o avia.o

AVIA_OBJS = avia_core.o
AVIA_OSD  = avia_osd.o

GTXENX_CORE  = gtxenx-core.o
GTXENX_DMX   = gtxenx-dmx.o crc32.o
GTXENX_FB    = gtxenx-fb.o fbgen.o

EXTRA_CFLAGS=-I../include -DEXPORT_SYMTAB

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(AVIA_OBJS) $(AVIA_OSD)	\
		$(GTXENX_DMX) $(GTXENX_CORE) $(GTXENX_FB) .*.o.flags *~

avia.o: $(AVIA_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OBJS)

osd.o: $(AVIA_OSD)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OSD)

gtxenx_core.o: $(GTXENX_CORE)
	$(LD) -m elf32ppclinux -r -o $@ $(GTXENX_CORE)

gtxenx_dmx.o: $(GTXENX_DMX)
	$(LD) -m elf32ppclinux -r -o $@ $(GTXENX_DMX)

gtxenx_fb.o: $(GTXENX_FB)
	$(LD) -m elf32ppclinux -r -o $@ $(GTXENX_FB)

include $(KERNEL_LOCATION)/Rules.make
