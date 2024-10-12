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

#include <iostream>
#include <stdio.h>
#include "Const.h"
#include "FilesIO.h"
#include "DgvMain.h"
#include "WavOut.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#ifdef _WIN32 // __unix__
	#include <windows.h>
#endif


//-------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------
struct WavHeader_Struct WavOutHeader;
struct WavSubchunk_Struct WavOutSubChunks[NChunkMax];
char WavOut_NameOptions[OptionsLenMax + 2];
#define NumErri64 0x80000000 // Invalid value if error in conversion for a 

//-------------------------------------------------------------------------
// Local variables
//-------------------------------------------------------------------------
// Some variables are not passed in functions to optimize functions in microcontrollers

//---------------
// Debug variables 
// Time and Delays are in Cpu Cycles (not necessarily an integer)
uint64_t Glob_Debug_K7ReadTime; // For debug only
uint64_t Glob_Debug_K7ReadTime_FirstInByte; // For debug only, end of Last Read K7 instruction in first loop of Period0 
uint64_t Glob_Debug_K7ReadTime_LastInByte; // For debug only, end of Last Read K7 instruction in first loop of Period0 
uint64_t Glob_Debug_WrittenSamplesinByte;

// Margin variables to generate wav for a Physical DAI
int16_t PeriodsOffset_Delay[DaiBitPeriod_Count];
int16_t InBkInterCallsDelaysMargin[PosInBlock_Count];
int16_t OutBkInterCallsDelaysMargin[PosInFile_Count];

// Wav related variables
FILE* WavOutFile;
uint8_t WavOut_NChannels;
uint8_t WavOut_Bytes_per_sample;
uint8_t WavOut_InvertSignal;
uint32_t WavOut_SamplingFq;
uint16_t WavOut_LeaderDaiBits ;
uint16_t WavOut_TrailerDaiBits ;
uint32_t WavOut_SamplesCount; // Actual samples count written to Wav file
uint16_t WavOut_SignalLevels[2][2]; // Samples levels definitions, WavOut_SignalLevels[LevelChange][TTLlevel]


//-------------------------------------------------------------------------
// Local functions
//-------------------------------------------------------------------------

int16_t WriteDaiByte(uint8_t DataByte);
int16_t WriteDaiBit(uint8_t DaiBitType, uint16_t InterCallsK7ReadDelay);
uint16_t WavSamplesMin(uint16_t CyclesMin);
int16_t WriteWavSamples(uint16_t Samples, uint8_t DaiBitPeriod);
int16_t WavOutLevel(uint8_t DaiBitPeriod, uint16_t SampleI);
uint16_t DaiBitLoopRelatedDelay(uint16_t DaiBitPeriod, uint16_t LoopCount);
int16_t WriteDaiTails(void);
int16_t WriteDaiCore(void);
int64_t GetFirstNumberInString(char* StringWithNum);

//=========================================================================
// FUNCTIONS
//=========================================================================

//-------------------------------------------------------------------------
// SetWavOutParameters 
//-------------------------------------------------------------------------
// Parameters to be used DgvOut
void SetWavOutParameters(uint16_t Hw)
{
	Glob_DaiHw = Hw ;
	uint8_t Px;
	uint32_t TailDaiBitDelay;

	// Margin variables to generate wav for a Physical DAI
	for (uint8_t PeriodI = 0; PeriodI < DaiBitPeriod_Count; PeriodI++)
	{
		PeriodsOffset_Delay[PeriodI] = DaiHW_Profile[Glob_DaiHw].PeriodsOffset_HwDelay[PeriodI];
	}
	for (uint8_t PosInBk = 0; PosInBk < PosInBlock_Count; PosInBk++)
	{
		InBkInterCallsDelaysMargin[PosInBk] = 0; // Intra Calls related sections, must be High enough to add a Sample (21 at 96KHz)
		OutBkInterCallsDelaysMargin[PosInBk] = 0; // Inter Calls related sections, must be High enough to add a Sample(21 at 96KHz)
	}

	WavOut_NChannels = DaiHW_Profile[Glob_DaiHw].NChannels;
	WavOut_Bytes_per_sample = DaiHW_Profile[Glob_DaiHw].Bytes_per_sample;
	WavOut_InvertSignal = DaiHW_Profile[Glob_DaiHw].InvertSignal;
	WavOut_SamplingFq = DaiHW_Profile[Glob_DaiHw].SamplingFq;
	WavOut_InvertSignal = DaiHW_Profile[Glob_DaiHw].InvertSignal;
	TailDaiBitDelay = 0 ;
	for (Px = DaiBit_P0_TTLL; Px <= DaiBit_P3_TTLH; Px++)
	{
		TailDaiBitDelay += DaiHW_Profile[Glob_DaiHw].DaiBitPeriods_MinLoops[DaiBitType_Leader][Px]*TailsCyclesPerLoop;
	}
	WavOut_LeaderDaiBits = (uint16_t) (DaiHW_Profile[Glob_DaiHw].Leader_ms * CpuFq / 1000 / TailDaiBitDelay) ;
	for (Px = DaiBit_P0_TTLL; Px <= DaiBit_P3_TTLH; Px++)
	{
		TailDaiBitDelay += DaiHW_Profile[Glob_DaiHw].DaiBitPeriods_MinLoops[DaiBitType_Trailer][Px] * TailsCyclesPerLoop;
	}
	WavOut_TrailerDaiBits = (uint16_t) (DaiHW_Profile[Glob_DaiHw].Trailer_ms * CpuFq / 1000 / TailDaiBitDelay + 1) ;

	Update_WavOut_NameOptions (WavOut_NameOptions); 

	// Levels to change depending of type of wav file (see WavOut_SignalLevels[2][2])
	for (uint8_t Levels = 0; Levels < 2; Levels++)
	{
		for (uint8_t TTL = 0; TTL < 2; TTL++)
		{
			WavOut_SignalLevels[Levels][TTL] = (WavOut_Bytes_per_sample == 2 ? WavLevels_2B[Levels][TTL] : WavLevels_1B[Levels][TTL]);
		}
	}
}

//-------------------------------------------------------------------------
// WriteDaiByte 
//-------------------------------------------------------------------------
// Convert a byte in wave samples
// Input:
// - Byte to write as a DaiBit
// - DaiBitType (global variables) - (fast or slow) x (High or low), see enum definition 
//   Slow / Fast Daibits differs by the number of minimum loops when reading K7 (defined by DaiBitPeriod_MinLoops)
// - Glob_InterK7ReadDelay (global variables) : delay between last K7 read (instruction end), and next instruction read (included) 
int16_t WriteDaiByte(uint8_t DataByte)
{
	uint8_t BitMask;
	int16_t Err;
	uint8_t DaiBitType;

	Glob_Debug_K7ReadTime_FirstInByte = Glob_Debug_K7ReadTime + Glob_InterK7ReadDelay ;
	Glob_Debug_WrittenSamplesinByte = 0;

	for (BitMask = 0x80; BitMask != 0; BitMask = BitMask >> 1)
	{
		if ((Glob_PosInFile > PosInFile_ProgByte)&&(Glob_BlockI > 0))
		// Current byte is part of a block (excpet the last byte)
		{
			DaiBitType = DaiBitType_LowFast;
		}
		else 
		// Normal bits
		{
			DaiBitType = DaiBitType_LowNorm;
		}
		DaiBitType += ((DataByte & BitMask) != 0); // Add 1 if bit is 1
		Err = WriteDaiBit(DaiBitType,Glob_InterK7ReadDelay ); if (Err < 0) break;

		// Delay between reading bits, if not last bit
		Glob_InterK7ReadDelay = ExitDaiBit_Delay + InterDaitBits_Delay + EnterDaiBit_Delay;
	}

	// Calculate Glob_InterK7ReadDelay : delays between last read Sample and next one for writing next byte
	if (Glob_PosInFile != PosInFile_InBlock)
	{	// Delay between bytes when not in Block
		Glob_InterK7ReadDelay = ExitDaiBit_Delay + EnterDaiBit_Delay + OutBkInterCallsDelays[Glob_ProgType-0x30][Glob_PosInFile];
		Glob_InterK7ReadDelay += OutBkInterCallsDelaysMargin[Glob_PosInFile];
	}
	else
	{	// Delay between trying to read last sample of a byte and 1st sample of a byte
		Glob_InterK7ReadDelay = ExitDaiBit_Delay + InBkInterCallsDelays[Glob_BlockI][Glob_PosInBlock] + EnterDaiBit_Delay; 
		Glob_InterK7ReadDelay += InBkInterCallsDelaysMargin[Glob_PosInBlock];
	}
	Glob_Debug_K7ReadTime_LastInByte = Glob_Debug_K7ReadTime;
#if(0) // For debug
	// printf("b0s= %06d,b7e= %06d,D= %06d,B= x%02X,S= %03d \n", (uint32_t)Glob_Debug_K7ReadTime_FirstInByte, (uint32_t)Glob_Debug_K7ReadTime_LastInByte, (uint32_t)Glob_InterK7ReadDelay, DataByte, (uint32_t)Glob_Debug_WrittenSamplesinByte);
#endif
	return(Err);
}


//-------------------------------------------------------------------------
// WriteDaiBit 
//-------------------------------------------------------------------------
// Convert a DaiBit, which has 4 periods from 0 to 3, in wave samples
int16_t WriteDaiBit(uint8_t DaiBitType, uint16_t InterCallsK7ReadDelay)
{
	uint8_t DaiBitPeriod;
	uint16_t RequiredPeriodMinDelay;
	uint16_t NSamplesMin;
	int16_t Err;

	for (DaiBitPeriod = 0; DaiBitPeriod < DaiBitPeriod_Count; DaiBitPeriod++)
	{
		// Loop related part. If Period0 and only 1 loop -> 0 because it is already included in InterCallsK7ReadDelay
		// Add margin for analog wav. Not allowed for Leader or trailer
		if ((Glob_PosInFile != PosInFile_Leader)&& (Glob_PosInFile != PosInFile_Trailer))
		{
			RequiredPeriodMinDelay = DaiBitLoopRelatedDelay(DaiBitPeriod, DaiHW_Profile[Glob_DaiHw].DaiBitPeriods_MinLoops[DaiBitType][DaiBitPeriod]);
			RequiredPeriodMinDelay += PeriodsOffset_Delay[DaiBitPeriod];
			if (DaiBitPeriod == 0)
			{
				RequiredPeriodMinDelay += InterCallsK7ReadDelay;
			}
		}
		else
		{
			// Header / Trailer timing is approximative
			RequiredPeriodMinDelay = TailsCyclesPerLoop * DaiHW_Profile[Glob_DaiHw].DaiBitPeriods_MinLoops[DaiBitType][DaiBitPeriod];
		}

		// Cycle count should be exact
		NSamplesMin = WavSamplesMin(RequiredPeriodMinDelay);
#if(0) // For debug
		if (Glob_BlockI == 1)
		{
			printf("K7T= %06d,bP= %02d,D= %04d,bT= %02d,NS= %02d\n", (uint32_t)Glob_Debug_K7ReadTime_FirstInByte, (uint8_t)DaiBitPeriod, (uint16_t)RequiredPeriodMinDelay, (uint8_t)DaiBitType, (uint8_t)NSamplesMin);
		}
#endif
		Err = WriteWavSamples(NSamplesMin, DaiBitPeriod);

		Glob_Debug_WrittenSamplesinByte += NSamplesMin;
		Glob_Debug_K7ReadTime += RequiredPeriodMinDelay;
		if (Err < 0) break;
	}
	return(Err);
}


//-------------------------------------------------------------------------
// WavSamplesMin
//-------------------------------------------------------------------------
// Calculates how many Samples should be written to catch start of next DaiBitPeriod
uint16_t WavSamplesMin(uint16_t CyclesMin)
{
	uint64_t Samples;
	Samples = (CyclesMin * WavOut_SamplingFq) / CpuFq;
	if (((Samples * CpuFq) != (CyclesMin * WavOut_SamplingFq))||(Samples==0))
	{
		Samples++ ;
	}
	return ((uint16_t) Samples);
}


//-------------------------------------------------------------------------
// WriteWavSamples 
//-------------------------------------------------------------------------
// Adds a sample in the output files 
int16_t WriteWavSamples(uint16_t Samples,uint8_t DaiBitPeriod)
{
	uint16_t Sample;
	int16_t WavLevel;
	uint8_t Ch;

	for (Sample = 0; Sample < Samples; Sample++)
	{
		WavLevel = WavOutLevel(DaiBitPeriod,Sample);
		for (Ch=0;Ch< WavOut_NChannels;Ch++)
		{
			if (fwrite(&WavLevel, WavOut_Bytes_per_sample, 1, WavOutFile) < 1)
			{
				return (-WavWriteErr);
			}
			WavOut_SamplesCount++;
		}
	}
	return (0);
}


//-------------------------------------------------------------------------
// WavLevel 
//-------------------------------------------------------------------------
// Set Wav Level according to DaiBitPeriod and WavOut_SmoothSignal
// Set WavOut_SmoothSignal to 0,to have a square signl
// Set WavOut_SmoothSignal to 1, for a smoother analog signal (used for a physical DAI)
int16_t WavOutLevel(uint8_t DaiBitPeriod,uint16_t SampleI)
{
	uint16_t Smoothed = 0;
	uint8_t TTLIndex_Low;
	uint8_t TTLIndex_High;

	if (WavOut_InvertSignal!=0)
	{
		TTLIndex_Low = 1;
		TTLIndex_High = 0;
	}
	else
	{
		TTLIndex_Low = 0;
		TTLIndex_High = 1;
	}

	#if(WavOut_SmoothSignal)
		if (SampleI==0) Smoothed = 1; // 
	#endif
	if ((DaiBitPeriod == 0) || (DaiBitPeriod == 2))
	{	
		return (WavOut_SignalLevels[Smoothed][TTLIndex_Low]);
	}
	else if ((DaiBitPeriod == 1) || (DaiBitPeriod == 3) )
	{
		return (WavOut_SignalLevels[Smoothed][TTLIndex_High]);
	}
	else 
	{
		return (128);
	}
}


//-------------------------------------------------------------------------
// DaiBitLoopRelatedDelay 
//-------------------------------------------------------------------------
// Input : Period (from 0 to 3) in a DaiBit (Read Bit function in firmware manual)
// Period 0 starts at the begining of the DaiBit (including call) and ends at the end of the read K7 instruction (Ora M) resulting in Sample Level transition
// Period 3 ends at the end of the read K7 instruction (Ana M) of the last loop of the DaiBit 
// Excludes InBkInterCallsDelays[]
uint16_t DaiBitLoopRelatedDelay(uint16_t DaiBitPeriod, uint16_t LoopCount)
{
	// DaiBitPeriod (delays from ReadBit start including call delay) DBP
	//	0: From call begining 
	//		 to K7 port read DBP 0 (1st DBP waiting for high level, end of "ORA M") 
	//  1: From K7 port read DBP 0 (1st DBP waiting for high level, end of "ORA M") 
	//		 to K7 port read DBP 1 (2nd DBP waiting for low level, end of "ANA M") 
	//  2: From K7 port read DBP 1 (2nd DBP waiting for low level, end of "ANA M") 
	//		 to K7 port read DBP 2 (3rd DBP waiting for low level, end of "ORA M") 
	//  3: From K7 port read DBP 2 (3rd DBP waiting for low level, end of "ORA M")
	//		 to K7 port read DBP 3 (4th DBP waiting for low level, end of "ANA M") 
	//  3: From K7 port read DBP 3 (4th DBP waiting for low level, end of "ANA M") 
	//		 to exit including RET

	uint16_t CpuCycles;

	CpuCycles = DaiBitCyclesPerLoop[DaiBitPeriod] * LoopCount;
	if (DaiBitPeriod == DaiBit_P0_TTLL)
	{
		CpuCycles -= DaiBitCyclesPerLoop[DaiBit_P0_TTLL]; // One lopp is already included in InterCallsK7ReadDelay
	}
	else if (DaiBitPeriod == DaiBit_P2_TTLL)
	{
		CpuCycles += 10;
	} 
	return (CpuCycles);
}


//-------------------------------------------------------------------------
// WriteBitTails : write Leader or Trailer
//-------------------------------------------------------------------------
// Write Leader (excluding SyncBit) or Trailer
int16_t WriteDaiTails(void)
{
	uint32_t N ;
	int16_t Err = 0;
	uint32_t Bit_Count ;
	uint8_t DaiBitT ;

	if (Glob_PosInFile==PosInFile_Trailer)
	{
		DaiBitT = DaiBitType_Trailer;
		Bit_Count = WavOut_TrailerDaiBits;
	}
	else
	{
		DaiBitT = DaiBitType_Leader;
		Bit_Count = WavOut_LeaderDaiBits;
	}

	for (N=0; N < Bit_Count; N++)
	{
		Err = WriteDaiBit(DaiBitT, 0);
		if (Err < 0) break;
	}
	return(Err);
}


//-------------------------------------------------------------------------
// WriteDaiCore 
//-------------------------------------------------------------------------
// Write all information to a wav file excluding Leader and Trailer
// Input: WavOutFile and global variables o/w Glob_ProgType
// Output : 0 or negative error code
int16_t WriteDaiCore(void)
{
	uint16_t DataI;
	int16_t nerr = 0;
	
	Glob_PosInFile = PosInFile_ProgByte;
	nerr = WriteDaiByte(Glob_ProgType); if (nerr < 0) { return (-WavWriteBlockErr); } // Write 0X55
	for (Glob_BlockI = 0; Glob_BlockI < DataBlock_Count; Glob_BlockI++)
	{
		Glob_PosInFile = PosInFile_InBlock;
		// Write Len and checksum of len
		Glob_PosInBlock = PosInBlock_LenH;
		nerr = WriteDaiByte((DaiBlocksInfo[Glob_BlockI].Len) >> 8); if (nerr < 0) { return (-WavWriteBlockErr); }
		Glob_PosInBlock = PosInBlock_LenL;
		nerr = WriteDaiByte((DaiBlocksInfo[Glob_BlockI].Len) & 0xFF); if (nerr < 0) { return (-WavWriteBlockErr); }
		Glob_PosInBlock = PosInBlock_LenCS;
		nerr = WriteDaiByte(DaiBlocksInfo[Glob_BlockI].LenCS); if (nerr < 0) { return (-WavWriteBlockErr); }
		Glob_PosInBlock = PosInBlock_InData;
	
		for (DataI = 0; DataI < DaiBlocksInfo[Glob_BlockI].Len; DataI++)
		{
			if (DataI > 0)
			{
				Glob_PosInBlock = PosInBlock_InData;
			} else
			if (DataI == (DaiBlocksInfo[Glob_BlockI].Len - 1))
			{
				Glob_PosInBlock = PosInBlock_LastByte;
			}
			nerr = WriteDaiByte(DaiBlocksInfo[Glob_BlockI].Block[DataI]); if (nerr < 0) { return (-WavWriteBlockErr); }
		}

		if (Glob_BlockI == 0)
		{
			Glob_PosInFile = PosInFile_BlockCS0;
		}
		else
		{
			Glob_PosInFile = PosInFile_BlockCSN;
		}
		nerr = WriteDaiByte(DaiBlocksInfo[Glob_BlockI].BlockCS); if (nerr < 0) { return (-WavWriteBlockErr); }
	}
	return(0);
}

//========================================================================


//-------------------------------------------------------------------------
// DgvWavOut
//-------------------------------------------------------------------------
// Reads a .dai file and convert it into a wav file
// Resulting files is ONLY for MAME emulator
// As the 750ms for starting the motor is not modelized, the LOAD or R functions has to be entered before opening the wav file

int16_t DgvWavOut(char* WavFileName)
{
	FILE* DaiFile = NULL;
	int16_t NErr = 0;
	
	Glob_PosInFile = PosInFile_Leader;
	Glob_BlockI = 0;
	WavOut_SamplesCount = 0;

    WavOutFile = fopen(WavFileName, "wb");
	if (WavOutFile == NULL) { NErr = - WavOpenErr; goto DgvWavExit2; }

	NErr = CreateWavOut(WavOutFile,0, WavOut_SamplingFq, WavOut_NChannels, WavOut_Bytes_per_sample);
	if (NErr < 0) { goto DgvWavExit2; }

	// Write Leader, a SyncBit and a StartByte (0x55), 3 Blocks and a Trailer 
	Glob_InterK7ReadDelay = EnterDaiBit_Delay;
	NErr = WriteDaiTails(); if (NErr < 0) { goto DgvWavExit; }
	Glob_InterK7ReadDelay = TailsCyclesPerLoop * DaiHW_Profile[Glob_DaiHw].DaiBitPeriods_MinLoops[DaiBitType_Leader][DaiBit_P3_TTLH]; // Delay of last Period (Low Level) of last Loop
	if (Glob_InterK7ReadDelay < LeaderLastBitToSyncBitDelayMin) 
	{
		Glob_InterK7ReadDelay = LeaderLastBitToSyncBitDelayMin;
	}
	NErr = WriteDaiBit(DaiBitType_SyncBit, Glob_InterK7ReadDelay); if (NErr < 0) { goto DgvWavExit; } // SyncBit, thefore no additional delay is required
	Glob_InterK7ReadDelay = SyncBitExit_Delay + SyncBitDaiBit_Delay + EnterDaiBit_Delay; // XXX No margin added, No need to interrupt delays because there are allow only if error 
	Glob_Debug_K7ReadTime = EnterDaiBit_Delay ; // XXXX
	Glob_PosInFile = PosInFile_SyncByte;
	NErr = WriteDaiByte(0x55); if (NErr < 0) { goto DgvWavExit; }
	NErr = WriteDaiCore(); if (NErr < 0) { goto DgvWavExit; }
	Glob_PosInFile = PosInFile_Trailer;
	NErr = WriteDaiTails(); if (NErr < 0) { goto DgvWavExit; }
	NErr = UdpdateWavSize(WavOutFile, WavOut_SamplesCount, WavOut_Bytes_per_sample); if (NErr < 0) { goto DgvWavExit; }

DgvWavExit:
DgvWavExit2:
	if (WavOutFile != NULL) { fclose(WavOutFile);	}

	return (NErr);
}


//-------------------------------------------------------------------------
// LoadProgOptionsArgument 
//-------------------------------------------------------------------------
// Update Wav out file options
// Input : Partial list of Options (string starting by -- with groups of characters, see PrinHelp for more details)
// Output : parameters listed in Options, read from program argument, are set
//			A flag corresponding to each option is coded in UpdatedOptionBits
//
uint16_t LoadProgOptionsArgument(char* Options)
{
	uint16_t UpdatedOptionBits = 0;
	int64_t OptVal;
	char* Opt2;
	uint16_t LenOpt;

	LenOpt = (uint16_t)strlen(Options);
	if ((LenOpt < 3) || (LenOpt > OptionsLenMax) || (Options[0] != '-') || (Options[1] != '-')) // Option validated with at least 1 char ('--x')
	{
		goto UpdateParamExit;
	}
	UpdatedOptionBits = OptionBit_OptionArgument ;

	// Set Glob_DaiHw ?
	Opt2 = strrchr(Options, 'V');
	if ( (Opt2 != NULL) && (((uint8_t)(Opt2+1-Options))<=LenOpt) ) // If V exist and at least one char is present after 
	{
		OptVal = * (Opt2+1);
		if ((OptVal >= '0') && (OptVal <= '9'))
		{ 
			Glob_DaiHw = (uint8_t) (OptVal - '0'); 
			SetWavOutParameters(Glob_DaiHw);
			UpdatedOptionBits |= OptionBit_Hardware;

		} // Ex 1 for DaiHW_DaiV7
	}

	// Set Byte count per sample, Mono/Stereo, Inversed signal (parity)
	if (strrchr(Options, 'M') != NULL) { WavOut_NChannels = 1;  UpdatedOptionBits |= OptionBit_NChannels; }
	if (strrchr(Options, 'S') != NULL) { WavOut_NChannels = 2; UpdatedOptionBits |= OptionBit_NChannels; }
	if (strrchr(Options, 'B') != NULL) { WavOut_Bytes_per_sample = 1; UpdatedOptionBits |= OptionBit_NBytes; }
	if (strrchr(Options, 'W') != NULL) { WavOut_Bytes_per_sample = 2; UpdatedOptionBits |= OptionBit_NBytes; }
	if (strrchr(Options, 'N') != NULL) { WavOut_InvertSignal = 0; UpdatedOptionBits |= OptionBit_Parity; }
	if (strrchr(Options, 'I') != NULL) { WavOut_InvertSignal = 1; UpdatedOptionBits |= OptionBit_Parity; }

	// Sampling frequency option
	Opt2 = strrchr(Options, 'F');
	if (Opt2 == NULL) goto UpdateParamExit;
	Opt2++;
	if (((uint8_t)(Opt2+5 - Options)) <= LenOpt) // If V exist and at least 5 char is present after  
	{
		OptVal = GetFirstNumberInString(Opt2);
		if ((OptVal >=20000L) && (OptVal <= 1000000L)) 
		{
			WavOut_SamplingFq = (uint32_t)(OptVal);
			UpdatedOptionBits |= OptionBit_Frequency;
		}
	}

UpdateParamExit:

	return (UpdatedOptionBits);
}


//-------------------------------------------------------------------------
// Update_WavOut_NameOptions 
//-------------------------------------------------------------------------
// Creates an Options string corresponding to actual parameters (inverse of LoadProgOptionsArgument)
// Output : Options is cleand to includes all parameters information
void Update_WavOut_NameOptions(char* Options)
{
	strcpy(Options, (char*)"--Vx");

	// Hardware type
	if (Glob_DaiHw < DaiHW_Count) { Options[3] = Glob_DaiHw + '0'; }

	// Mono or Stereo
	if (WavOut_NChannels == 2)
	{
		strcat(Options, (char*)"S");
	}
	else
	{
		strcat(Options, (char*)"M");
	}

	// 8 bits or 16 Bits per sample
	if (WavOut_Bytes_per_sample == 2)
	{
		strcat(Options, (char*)"W");
	}
	else
	{
		strcat(Options, (char*)"B");
	}


	// Normal or inverted signal
	if (WavOut_InvertSignal == 0)
	{
		strcat(Options, (char*)"N");
	}
	else
	{
		strcat(Options, (char*)"I");
	}

	// Frequency
	strcat(Options, (char*)"F");
	sprintf(Options + strlen(Options), "%ld", WavOut_SamplingFq);
}


//-------------------------------------------------------------------------
// GetFirstNumberInString 
//-------------------------------------------------------------------------
// Input valid string, with potential a number with a maximum of NumberMaxChars characters
// Output : first number (signed) found in string
int64_t GetFirstNumberInString(char* StringWithNum)
{
	char* StartChar;
	char* EndChar;
	int64_t Num;

	StartChar = StringWithNum;
	while (((*StartChar) != '\0') && (((*StartChar) < '0') || ((*StartChar) > '9'))) ;
	if ((*StartChar) == '\0') return (NumErri64);
	Num = strtoll(StartChar, &EndChar, 10);

	// Check if negative
	StartChar--;
	if ( (StartChar>=StringWithNum) && ((*StartChar)=='-') )
	{
		Num = -Num;
	}
	return (Num);
}
