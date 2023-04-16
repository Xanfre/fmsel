objdir = ./objs

TARGET ?=
ifeq ($(TARGET),)
CC = gcc
CXX = g++
else
CC = $(TARGET)-gcc
CXX = $(TARGET)-g++
endif

CFLAGS_ALL = -O2 $(CFLAGS)
CXXFLAGS_ALL = -O2 $(CXXFLAGS)
CPPFLAGS_ALL = -std=gnu++11 -Wno-deprecated-declarations -Wno-stringop-overflow $(CPPFLAGS)
LDFLAGS_ALL = fmsel.def $(LDFLAGS)

LIBS =  -lfltk -lfltk_images -lfltk_jpeg -l7zip -lvorbisfile -lvorbis -logg

ifeq ($(STATIC),)
	LIBS += -loleaut32 -luuid -lcomdlg32 -lgdi32
else
	LIBS += -loleaut32 -lole32 -luuid -lcomctl32 -lcomdlg32 -lgdi32 -static
endif
#-lole32 -loleaut32 -loleaut32 -lole32 -luuid -lwinspool -lwinmm -lshell32 -lcomctl32 -lcomdlg32 -lctl3d32 -ladvapi32 -lwsock32 -lgdi32 -lkernel32 -luser32

FMSEL_OBJS = $(objdir)/archive.o \
			$(objdir)/Fl_Html_View.o \
			$(objdir)/lang.o \
			$(objdir)/mp3.o \
			$(objdir)/os.o \
			$(objdir)/fmsel.o
FLTK_OBJS = $(objdir)/Fl_Table.o \
			$(objdir)/Fl_Table_Row.o
ZLIB_OBJS = $(objdir)/adler32.o \
			$(objdir)/compress.o \
			$(objdir)/crc32.o \
			$(objdir)/deflate.o \
			$(objdir)/ioapi.o \
			$(objdir)/iowin32.o \
			$(objdir)/trees.o \
			$(objdir)/zip.o \
			$(objdir)/zutil.o

all: $(objdir) fmsel.dll

clean:
	rm -rf $(objdir)
	rm -f fmsel.dll

$(objdir):
	mkdir -p $@

$(objdir)/%.o: ./Fl_Table/%.cxx
	$(CXX) $(CXXFLAGS_ALL) $(CPPFLAGS_ALL) -c -o $@ $<

$(objdir)/%.o: ../zlibcomp/%.c
	$(CC) $(CFLAGS_ALL) $(CPPFLAGS) -c -o $@ $<

$(objdir)/%.o: %.cpp
	$(CXX) $(CXXFLAGS_ALL) $(CPPFLAGS_ALL) -I./Fl_Table -I../zlibcomp -c -o $@ $<

fmsel.dll: $(FMSEL_OBJS) $(FLTK_OBJS) $(ZLIB_OBJS)
	$(CXX) $(CXXFLAGS_ALL) -shared $(LDFLAGS_ALL) $^ -o $@ $(LIBS)
