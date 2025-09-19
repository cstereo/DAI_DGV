# DGV

<img width="1133" height="510" alt="Max compression" src="https://github.com/user-attachments/assets/e0ec5d09-395e-40b1-a5e8-348896c9f97c" />

## DAI vintage computer optimized wav program generator

DGV is a small program that converts DAI programs (a vintage computer designed in 1978) into “optimized wav” files.
Loading speed on a physical DAI can be up 7.7x faster than with original audio files and 9.4x on a Mame emulator.
Compression relies on the cassette signal transitions required to match CPU read instructions and interruptions related jitter.
When using a real DAI computer, a specific compression profile takes into account delays introduced by electronic components.    

Files inputs can be “.wav” audio analog files or DAI “.dai” binary files. 

DGV is written in C, and is compiled here with VS Code and MSVC compiler.
It does not have any dependency outside the C17 standard.


## Environment Variables
Following System variables needs to be set if you want to compile the program
- "MSVC_VS2022", set for example to "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120"
- "WindowsKitsSdk", set for example to "C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0"

