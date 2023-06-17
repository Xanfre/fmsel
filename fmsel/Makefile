objdir = ./objs

TARGET ?=
ifeq ($(TARGET),)
CC = gcc
CXX = g++
else
CC = $(TARGET)-gcc
CXX = $(TARGET)-g++
endif

TARGET_OS ?= $(OS)
ifneq ($(TARGET_OS),Windows_NT)
CFLAGS_ALL = -O2 -fPIC
CXXFLAGS_ALL = -O2 -fPIC
else
CFLAGS_ALL = -O2
CXXFLAGS_ALL = -O2
endif

CFLAGS_ALL += $(L7ZIP_INC) $(CFLAGS)
CXXFLAGS_ALL += $(L7ZIP_INC) $(CXXFLAGS)
CPPFLAGS_ALL = -std=gnu++11 -Wall -Wno-unused-variable -Wno-unused-result -Wno-format-security $(CPPFLAGS)
LDFLAGS_ALL = $(L7ZIP_LIB) $(LDFLAGS)

ifeq ($(TARGET_OS),Windows_NT)
	CPPFLAGS_ALL += -I../zlibcomp
ifneq ($(STATIC),)
	CPPFLAGS_ALL += -DSTATIC
endif
else
	CPPFLAGS_ALL += -D__int64="long long int"
endif

LIBS = -lfltk -lfltk_images -l7zip -lvorbisfile -lvorbis -logg

ifeq ($(TARGET_OS),Windows_NT)
	LIBS += -lfltk_jpeg
ifeq ($(STATIC),)
	LIBS += -loleaut32 -luuid -lcomdlg32 -lgdi32
else
	LIBS += -loleaut32 -lole32 -luuid -lcomctl32 -lcomdlg32 -lgdi32 -static
endif
ifeq ($(SONAME),)
	SONAME = fmsel.dll
endif
else
	LIBS += -lminizip -lX11
ifeq ($(SONAME),)
	SONAME = libfmsel.so
endif
	LDFLAGS_ALL += -Wl,-soname,$(SONAME)
endif

FMSEL_OBJS = $(objdir)/archive.o \
			$(objdir)/Fl_Html_View.o \
			$(objdir)/fmsel.o \
			$(objdir)/lang.o \
			$(objdir)/mp3.o \
			$(objdir)/os.o
FLTK_OBJS = $(objdir)/Fl_Table.o \
			$(objdir)/Fl_Table_Row.o

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

all: $(objdir) $(SONAME)

clean:
	rm -rf $(objdir)
	rm -f $(SONAME)

$(objdir):
	mkdir -p $@

$(objdir)/%.o: ./Fl_Table/%.cxx
	$(CXX) $(CXXFLAGS_ALL) $(CPPFLAGS_ALL) -c -o $@ $<

$(objdir)/%.o: ../zlibcomp/%.c
	$(CC) $(CFLAGS_ALL) $(CPPFLAGS) -c -o $@ $<

$(objdir)/%.o: %.cpp
	$(CXX) $(CXXFLAGS_ALL) $(CPPFLAGS_ALL) -I./Fl_Table -c -o $@ $<

$(SONAME): $(FMSEL_OBJS) $(FLTK_OBJS) $(ZLIB_OBJS)
	$(CXX) $(CXXFLAGS_ALL) -shared $(LDFLAGS_ALL) $^ -o $@ $(LIBS)
