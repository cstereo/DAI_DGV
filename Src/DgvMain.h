// MIT License

// Copyright(c) 2024 cstereo

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef TGVMAIN_H
#define TGVMAIN_H
#include <stdio.h>
#include <string.h> // for memcmp
#include <stdint.h> // for int16_t and int32_t definitions
#include "WavOut.h"


//-------------------------------------------------------------------------
// A DAI signal (wav file) is made of 
//		- Leader
//		- SyncBit
//		- SyncByte (0x55)
//		- ProgByte (0x30) to (0x32)
//		- Three Blocks made of
//			+ Datalen = 2 Bytes for block length
//   		+ LenCheckSum byte
//			+ DataBuffer of DataLen bytes (can be empty)
//			+ DataCheckSum byte
// 		- Trailer
//
// Nature of blocks depends on program type (Basic program, Binary program or Tables of data)
// First block always contains the Name of the program / table
//
// A (logic) Bit (DaiBit) contains 4 periods 
//		- DaiBitPeriod0 : period when looking for Sample level to go up. Includes in this program delays between DaiBits
//		- DaiBitPeriod1 : period when looking for Sample level to go down
//		- DaiBitPeriod2 : period when looking for Sample level to go up
//		- DaiBitPeriod3 : period when looking for Sample level to go down
//
// When writing a Byte, its nature is defined, in this program by global, varaibles Glob_PosInFile and Glob_PosInBlock (see enum definitions)
//
// Time and Delay are in Cpu cycles
// 
//-------------------------------------------------------------------------


//-------------------------------------------------------------------------
// USER Definitions
//-------------------------------------------------------------------------

#define UseWavIn 1 // Input Analog wav In


//-------------------------------------------------------------------------
// Global variables 
//-------------------------------------------------------------------------
extern int8_t Glob_PosInFile;
extern int8_t Glob_PosInBlock;
extern int8_t Glob_BlockI;
extern uint16_t Glob_InterK7ReadDelay; // Delay in CpuCycles between return and call of Read Bit function, including Enter & Exit delays
extern uint16_t Glob_DaiHw;

//-------------------------------------------------------------------------
// Global functions 
//-------------------------------------------------------------------------
void SetWavOutParameters(uint16_t Hw);
bool NotDgvFile(char* FileName);
uint16_t SwapBytes(uint16_t Word);
uint8_t DaiByteCheckSum(uint8_t Data, uint8_t ChkSum);
uint8_t DaiWordCheckSum(uint16_t Word);


#endif