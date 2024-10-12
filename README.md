# DGV
## DAI vintage computer optimized wav program generator

DGV is a small program that converts DAI programs (a vintage computer designed in 1978) into “optimized wav” files.
Loading speed on a physical DAI can be up 7.7x faster than with original audio files and 9.4x on a Mame emulator. 

Files inputs can be “.wav” audio analog files or DAI “.dai” binary files. 

DGV is written in C, and is compiled here with VS Code and MSVC compiler.
It does not have any dependency outside the C17 standard.


## Environment Variables
Following System variables needs to be set if you want to compile the program
- "MSVC_VS2022", set for example to "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120"
- "WindowsKitsSdk", set for example to "C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0"

