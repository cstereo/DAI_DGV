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

#ifndef CONST_H
#define CONST_H
#include <stdint.h> // for int16_t and int32_t


//-------------------------------------------------------------------------
// Program preferences
//-------------------------------------------------------------------------
#define InsertWavOutOptions 1 // Insert options to Wav out files name
#define MaxLenString 254 // 1 char required to store '\0'
#define OptionsLenMax 64

//-------------------------------------------------------------------------
// DAI Definitions DO NOT CHANGE
//-------------------------------------------------------------------------
// Timing of Dai firmware (ROM) READING functions, essentially in Cpu cycles 
// 
// File related
#define CpuFq 2000000LL
#define ProgType_Count 3 // Basic, Binary, Table

// Interrupts delays (examples of ADDED delays between K7 Read by interrupt routine)
#define Rst7_Clock_Delay 276 // 0xD9A9, RST 7, Clock interrupt (delay when timer is not zero and cursor is not blinked), (seen in Read Leader function, RDL10 loop looking for 1st drop)
#define Rst6A_Clock_Delay 301 // 0xD578, RST 6, may last 484, Keyboard interrupt (seen in Read Leader function, RDL10 loop looking for 1st drop)
#define Rst6B_Clock_Delay 484 // 0xD578, RST 6, may last 301, Keyboard interrupt (seen in Read Leader function, RDL10 loop looking for 1st drop)

#define Rst6Period_Delay (CpuFq/1000*16) // 16 ms, 32000 
#define Rst7Period_Delay (CpuFq/1000*20) // 20 ms, 40000

// Leader / Trailer related sections
#define LeaderMinHighLevelsForSync 0x14 // Additional loops are required for Interrupts, initial drop pulse (due to default length being 0x28), at least 2 SyncByte errors
#define LeaderLastBitToSyncBitDelayMin 98 //  

// Sync Byte section
#define SyncBitExit_Delay 50 // Delay between last Sample read (excluded) and return included 
#define SyncBitDaiBit_Delay 67 // 67 = from SyncBit return to DaiBit XXXX  
#define PostSyncByteFailure_Delay 367 // If failure to read 0x55 and no interrupt (see added interrupt delays below)

// Blocks related
#define DataBlock_Count 3 // Number of Blocks in a program file

// Byte related sections
#define InterDaitBits_Delay (38) // 38 delays between reading bits  (DaiBit)

// DaiBit related sections
#define EnterDaiBit_Delay 58 // Delay between call start and Sample read (included) of a Read Bit function
#define ExitDaiBit_Delay 25 //Delay between last Sample read (excluded) and return included of a Read Bit function


//-------------------------------------------------------------------------
// Enumerations
//-------------------------------------------------------------------------
// Cpu cycles delay of each bit type ProgBitType and section
enum DaiBitType { DaiBitType_LowFast, DaiBitType_HighFast, DaiBitType_LowNorm, DaiBitType_HighNorm, 
	                  DaiBitType_Leader, DaiBitType_Trailer, DaiBitType_SyncBit, DaiBitType_Count};
enum PosInBlock { PosInBlock_LenH, PosInBlock_LenL, PosInBlock_LenCS, PosInBlock_1stByte, PosInBlock_InData, PosInBlock_LastByte, PosInBlock_Count };
enum PosInFile { PosInFile_Leader, PosInFile_SyncByte, PosInFile_ProgByte, PosInFile_InBlock, PosInFile_BlockCS0, PosInFile_BlockCSN, PosInFile_Trailer, PosInFile_Count };
enum DaiBitPeriod { DaiBit_P0_TTLL, DaiBit_P1_TTLH, DaiBit_P2_TTLL, DaiBit_P3_TTLH, DaiBitPeriod_Count};
enum Errors {	
	HelpRequestErr = 1, InvalidCmdInputErr, InvalidCmdOption,
	WavOpenErr, DaiFileOpenErr, ReadDaiFileErr, ReadDaiDataErr, WriteDaiDataErr, InvalidDaiDataErr, DaiBlockCallocErr, DaiDataCheckSumErr,
	WavWriteErr, WavWriteBlockErr, DirectoryErr, FileParamErr, MemAllocErr, 
	WavInHeaderErr, WavInReadErr, EndOfFileErr, WavInLeaderErr, WavInSyncTypeErr, WavInProgTypeErr, WavInCallocErr, WavInBlockLenErr,
	WavInBlockLenCSErr, WavReadBlockErr, WavInBlockCSErr};

//-------------------------------------------------------------------------
// Global constants 
//-------------------------------------------------------------------------
// DataBlock x { LenH, LenL, LenCS, 1stByte, InData, LastByte }
const uint16_t InBkInterCallsDelays[DataBlock_Count][PosInBlock_Count] =
{ {211,194,432,435,435,435},
  {211,194,346,262,262,240},
  {211,194,346,262,262,240} };

// ProgType x { Leader, SyncByte, ProgByte, InBlock, BlockCS0, BlockCSN, Trailer }
const uint16_t OutBkInterCallsDelays[ProgType_Count][PosInFile_Count] =
{ {0,172,420,0,561,514,0},
  {0,172,336,0,603,571,0},
  {0,172,336,0,614,657,0} };

const uint16_t DaiBitCyclesPerLoop[DaiBitPeriod_Count] = { 32,37,32,37 };
const uint16_t TailsCyclesPerLoop = 32 ;// Header / Trailer timing is approximative, useless to improve this

#endif