objdir = ./objs

PREFIX ?= /usr
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
LICDIR = $(PREFIX)/share/licenses/libfmsel
INCDIR = $(PREFIX)/include

TARGET ?=
ifeq ($(TARGET),)
CC ?= gcc
CXX ?= g++
else
CC = $(TARGET)-gcc
CXX = $(TARGET)-g++
endif

TARGET_OS ?= $(OS)

CFLAGS_ALL = -O2 $(CFLAGS)
CXXFLAGS_ALL = -O2 $(CXXFLAGS)
CPPFLAGS_C_ALL = -std=c99 -Wall -Wno-unused $(CPPFLAGS)
CPPFLAGS_CXX_ALL = -std=gnu++11 $(L7ZIP_INC) -Wall -Wno-unused -Wno-format-security $(CPPFLAGS)
LDFLAGS_ALL = $(L7ZIP_LIB) $(LDFLAGS)

ifeq ($(TARGET_OS),Windows_NT)
	CPPFLAGS_CXX_ALL += -I../zlibcomp
ifneq ($(STATIC),)
	CPPFLAGS_CXX_ALL += -DSTATIC
endif
else
	CFLAGS_ALL += -fPIC
	CXXFLAGS_ALL += -fPIC
	CPPFLAGS_CXX_ALL += -D_LARGEFILE_SOURCE -DLARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DSYSTEM_UTF8 -D__int64="long long int"
endif
ifneq ($(SUPPORT_T3),)
	CPPFLAGS_CXX_ALL += -DSUPPORT_T3
endif
ifneq ($(SUPPORT_GLML),)
	CPPFLAGS_CXX_ALL += -DSUPPORT_GLML
endif

LIBS = -lfltk -lfltk_images -l7zip -lvorbisfile -lvorbis -logg

ifeq ($(TARGET_OS),Windows_NT)
	LIBS += -lfltk_jpeg
ifeq ($(STATIC),)
	LIBS += -loleaut32 -luuid -lcomdlg32 -lgdi32
else
	LIBS += -loleaut32 -lole32 -luuid -lcomctl32 -lcomdlg32 -lgdi32 -static
endif
	BINEXT = .exe
	SONAME ?= fmsel.dll
else
	LIBS += -lminizip -lX11
	SONAME ?= libfmsel.so
	LDFLAGS_ALL += -Wl,-soname,$(SONAME)
endif

TOOL_LIBS = -l:$(SONAME)

ifeq ($(TARGET_OS),Windows_NT)
ifneq ($(STATIC),)
	TOOL_LIBS += -static
endif
endif

FMSEL_OBJS = $(objdir)/archive.o \
			$(objdir)/Fl_Html_View.o \
			$(objdir)/fmsel.o \
			$(objdir)/lang.o \
			$(objdir)/mp3.o \
			$(objdir)/os.o
FLTK_OBJS = $(objdir)/Fl_Table.o \
			$(objdir)/Fl_Table_Row.o
TOOL_OBJS = $(objdir)/fmsel_launch.o

ifeq ($(TARGET_OS),Windows_NT)
ZLIB_OBJS = $(objdir)/adler32.o \
			$(objdir)/compress.o \
			$(objdir)/crc32.o \
			$(objdir)/deflate.o \
			$(objdir)/ioapi.o \
			$(objdir)/iowin32.o \
			$(objdir)/trees.o \
			$(objdir)/zip.o \
			$(objdir)/zutil.o
else
ZLIB_OBJS =
endif
ifneq ($(SUPPORT_T3),)
	FMSEL_OBJS += $(objdir)/glml.o
else ifneq ($(SUPPORT_GLML),)
	FMSEL_OBJS += $(objdir)/glml.o
endif

ifeq ($(BUILD_TOOLS),)
all: $(objdir) $(SONAME)
else
all: $(objdir) $(SONAME) fmsel$(BINEXT)
endif

install: install_link install_tools
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(LICDIR)
	mkdir -p $(DESTDIR)$(INCDIR)
	install -p -m 755 $(SONAME) $(DESTDIR)$(LIBDIR)
	install -p -m 644 COPYING $(DESTDIR)$(LICDIR)
	install -p -m 644 fmsel.h $(DESTDIR)$(INCDIR)

clean:
	rm -rf $(objdir)
	rm -f $(SONAME)
	rm -f fmsel$(BINEXT)

install_link:
ifneq ($(SONAME),libfmsel.so)
	ln -s $(SONAME) $(DESTDIR)$(LIBDIR)/libfmsel.so
endif

install_tools:
ifneq ($(BUILD_TOOLS),)
	mkdir -p $(DESTDIR)$(BINDIR)
	install -p -m 755 fmsel$(BINEXT) $(DESTDIR)$(BINDIR)
endif

$(objdir):
	mkdir -p $@

$(objdir)/%.o: ./Fl_Table/%.cxx
	$(CXX) $(CXXFLAGS_ALL) $(CPPFLAGS_CXX_ALL) -c -o $@ $<

$(objdir)/%.o: ../zlibcomp/%.c
	$(CC) $(CFLAGS_ALL) $(CPPFLAGS_C_ALL) -c -o $@ $<

$(objdir)/%.o: %.cpp
	$(CXX) $(CXXFLAGS_ALL) $(CPPFLAGS_CXX_ALL) -I./Fl_Table -c -o $@ $<

$(objdir)/%.o: %.c
	$(CC) $(CFLAGS_ALL) $(CPPFLAGS_C_ALL) -c -o $@ $<

$(SONAME): $(FMSEL_OBJS) $(FLTK_OBJS) $(ZLIB_OBJS)
	$(CXX) $(CXXFLAGS_ALL) -shared $(LDFLAGS_ALL) $^ -o $@ $(LIBS)

fmsel$(BINEXT): $(TOOL_OBJS) $(SONAME)
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -L. $(TOOL_OBJS) -o $@ $(TOOL_LIBS)
