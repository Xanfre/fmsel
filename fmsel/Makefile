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
CPPFLAGS_C_ALL = -std=c99 -Wall -Wextra -Wno-unused $(CPPFLAGS)
CPPFLAGS_CXX_ALL = -std=gnu++11 $(L7ZIP_INC) -Wall -Wextra -Wno-cast-function-type -Wno-format-security -Wno-unused $(CPPFLAGS)
LDFLAGS_ALL = $(L7ZIP_LIB) $(LDFLAGS)

ifeq ($(TARGET_OS),Windows_NT)
	CPPFLAGS_C_ALL += -DNO_VIZ
	CPPFLAGS_CXX_ALL += -I../zlibcomp
ifneq ($(STATIC),)
	CPPFLAGS_CXX_ALL += -DSTATIC
endif
else
	CFLAGS_ALL += -fPIC
	CXXFLAGS_ALL += -fPIC
	CPPFLAGS_CXX_ALL += -D_LARGEFILE_SOURCE -DLARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D__int64="long long int"
ifneq ($(SYSTEM_UTF8),)
	CPPFLAGS_CXX_ALL += -DSYSTEM_UTF8
endif
ifneq ($(USE_PATH_MAX),)
	CPPFLAGS_CXX_ALL += -DUSE_PATH_MAX
endif
ifneq ($(USE_XDG),)
	CPPFLAGS_CXX_ALL += -DUSE_XDG
endif
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

.PHONY: all install clean install_link install_tools

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
	mkdir -p $(DESTDIR)$(LIBDIR)
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

$(objdir)/archive.o: archive.h os.h lang.h dbgutil.h
$(objdir)/Fl_Html_View.o: Fl_Html_View.H flstring.h
$(objdir)/fmsel.o: fmsel.h os.h archive.h lang.h dbgutil.h glml.h Fl_Html_View.H ./Fl_Table/Fl_Table_Row.H ./Fl_Table/Fl_Table.H mp3.h icons.h fl_mainwnd.h fl_taged.h dark_cmap.h fl_mainwnd.cpp.h fl_taged.cpp.h
$(objdir)/lang.o: lang.h os.h dbgutil.h
$(objdir)/mp3.o: mp3.h os.h lang.h dbgutil.h
$(objdir)/os.o: lang.h os.h dbgutil.h
$(objdir)/glml.o: glml.h

$(objdir)/Fl_Table.o: ./Fl_Table/Fl_Table.H
$(objdir)/Fl_Table_Row.o: ./Fl_Table/Fl_Table_Row.H ./Fl_Table/Fl_Table.H

$(objdir)/fmsel_launch.o: fmsel.h

$(objdir)/adler32.o: ../zlibcomp/zutil.h ../zlibcomp/zlib.h ../zlibcomp/zconf.h
$(objdir)/compress.o: ../zlibcomp/zlib.h ../zlibcomp/zconf.h
$(objdir)/crc32.o: ../zlibcomp/zutil.h ../zlibcomp/zlib.h ../zlibcomp/zconf.h ../zlibcomp/crc32.h
$(objdir)/deflate.o: ../zlibcomp/deflate.h ../zlibcomp/zutil.h ../zlibcomp/zlib.h ../zlibcomp/zconf.h
$(objdir)/ioapi.o: ../zlibcomp/zlib.h ../zlibcomp/zconf.h ../zlibcomp/ioapi.h
$(objdir)/iowin32.o: ../zlibcomp/zlib.h ../zlibcomp/zconf.h ../zlibcomp/ioapi.h ../zlibcomp/iowin32.h
$(objdir)/trees.o: ../zlibcomp/deflate.h ../zlibcomp/zutil.h ../zlibcomp/zlib.h ../zlibcomp/zconf.h ../zlibcomp/trees.h
$(objdir)/zip.o: ../zlibcomp/zlib.h ../zlibcomp/zconf.h ../zlibcomp/zip.h ../zlibcomp/ioapi.h ../zlibcomp/crypt.h
$(objdir)/zutil.o: ../zlibcomp/zutil.h ../zlibcomp/zlib.h ../zlibcomp/zconf.h

$(SONAME): $(FMSEL_OBJS) $(FLTK_OBJS) $(ZLIB_OBJS)
	$(CXX) $(CXXFLAGS_ALL) -shared $(LDFLAGS_ALL) $^ -o $@ $(LIBS)

fmsel$(BINEXT): $(TOOL_OBJS) $(SONAME)
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -L. $(TOOL_OBJS) -o $@ $(TOOL_LIBS)
