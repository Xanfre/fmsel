## About
FMSel is a fan mission selector and manager for games based on the Dark Engine. Information pertaining to its use and operation can be found in `fmsel.odg`.  
This repository is a fork of the original release of FMSel included with the NewDark binaries. It is currently based on the latest original release.  

## Building
To build the source on Windows, you will need Visual Studio 2008 Professional with Service Pack 1 or MinGW-w64.  

The following additional software is needed.  
[FLTK](https://www.fltk.org/software.php)  
[vorbis](https://xiph.org/vorbis) (optional)  
[opus](https://opus-codec.org) (optional)  
[FLAC](https://xiph.org/flac) (optional)  
[dr_libs](https://github.com/mackron/dr_libs) (optional)  

Either have these libraries and their dependencies globally available to your compiler and linker or, if using Visual Studio, place them in the `fltk`, `ogg`, `opus`, `flac`, and `dr_libs` subdirectories, respectively. The headers should be in these directories themselves while the libraries should be in the `lib` subdirectory.  

Windows releases are built with Visual Studio 2008.  
