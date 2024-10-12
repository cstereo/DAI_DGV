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

#ifdef _WIN32 // __unix__
	#include <windows.h>
	#include <string.h>
#endif

#include "FilesIO.h"
#include"WavIn.h"
#include "WavOut.h"
#include "DgvMain.h"
#include <stdbool.h>

//-------------------------------------------------------------------------
// Introduction
//-------------------------------------------------------------------------
// Leader has three main section
//  1) Section A waits for a Low level (TTL High / Wav low as the operational amplifier inverts the signal), which is the level when no signal is present
//  2) Section B estimates each TTL high level length using a loop of 32 Cpu cycles
//  3) Section C (Sync bit) which simply waits for a High then Low level. 
// 
// Section B TTL length is estimated by the number of K7 read test, LoopI variable here, using a loop of 32 Cpu cycles. Assuming LoopI1, LoopI2 -> 2 consecutives delays. 
//		Leader pulse detection is assumed when (LoopI2 - LoopI1) > LoopI1 / 8. At least 20 valid consecutive detections are necessary
//		In that case, any "out of margin" / incorrect pulse detection
//		In practice, it means than LoopI should always be over 16 to allow acceptable difference of 2
//		For Wav files created by Mame at 44100KHz, High Level periods last from 12 to 14 samples, resulting into LoopI from 16 to 19 (20 is seldom) 
//		In practice, difference of three LoopI is relatively common, resulting in the risk of moving to section C  
//		Section C is detected if the previous high level was large enough to invalid the leader pulse detection test
//
// Read Leader firmware function success depends therefore on the timing difference between timing of entry and signal start.
// When sync bit is invalid (o/w when leader pulse has become slightly out of margin),...
//		some "random" delay may be added on function relaunch by interrupt and help modify this difference 
//
// Interrupt timings are approximative...

//-------------------------------------------------------------------------
// Definition
//-------------------------------------------------------------------------
#define RestartIfInvalidSyncByte  1 // 1, as coded in firmware, this restarts Read Leader when SyncByte is invalid 
#define AllowInterruptSimul 0 // 1, Allow to add delays every 20ms / 16 ms, similarly to a DAI 

// Simulations
#define CpuTimeStartOffset (-26) // -26 to have 0 on first Read,-26+306 to have same timing as MAME
#define CpuTimeStartOffset_VariationMin 0
#define CpuTimeStartOffset_VariationMax 0

// Interrupt
#define Init_Rst6_CpuTime (18420-Rst6Period_Delay) // 32000 per loop
#define Init_Rst7_CpuTime (38680-Rst7Period_Delay) // 40000 per loop

// Debug variables
#define WavIn_Display_Debug 0 // 1 for Loop level, 2 for Leader level, 3 for Cpu Start level, 0 no Debug
#define WavIn_Display_Interrupt 0 // Display timing of interrupts


//-------------------------------------------------------------------------
// Local variables
//-------------------------------------------------------------------------
FILE* WavInFile;
struct Wav_Struct CurrentWavIn = {};
struct WavHeader_Struct WavInHeader = {};
int16_t TriggerLevelDown;
int16_t TriggerLevelUp;
uint32_t WavInPosOffset;
uint32_t WavInPosMax;
bool WavIn_InvertSignal; // Inverted for a real Dai


//---------------
uint64_t Glob_CpuTime;
uint32_t Glob_BinByteI_Debug;
int16_t LoopL;
int16_t LoopH;
int16_t LoopHOld;
#if(WavIn_Display_Debug!=0)
	int16_t LoopHOld_Debug ;
#endif

//---------------
// Interrupt related
uint64_t Rst6_LastCpuTime ; // Triggered every 16ms, 0xD578 via RST 6
bool Rst6_NextDelayIsShort ; // Rst6 delay is alternatively short or long
uint64_t Rst7_LastCpuTime ; // Triggered every 20ms by TV page blanking signal, 0xD9A9 via RST 7 (clock interrupt)


//-------------------------------------------------------------------------
// Local functions
//-------------------------------------------------------------------------
int16_t ReadLeader(void);
int16_t ReadDaiCore(void);
int16_t ReadDaiByte(void);
int16_t ReadDaiBit(uint16_t InterCallsK7ReadDelay) ;
int16_t LevelChangeLoops(int16_t ExpectedTtlTrigger, uint16_t OffsetDelay, uint16_t LoopDelay, bool LimitDelay, bool IntEnabled) ;

uint16_t InterruptSimul_Delay(int16_t MinDelay);
uint16_t Rst7Simul_Delay(uint64_t EnabledCpuTime, uint16_t EnabledPeriod);
uint16_t Rst6Simul_Delay(uint64_t EnabledCpuTime, uint16_t EnabledPeriod);


//=========================================================================
// FUNCTIONS
//=========================================================================

//-------------------------------------------------------------------------
// LevelChangeLoops
//-------------------------------------------------------------------------
// Read wav until level change or end of file
// Input : 
//		ExpectedTtlTrigger = expected signal trigger leading to function exit
//		OffsetDelay: from current CpuTime before reading first Signal
//		LoopDelay : delay between each signal K7read
//		LimitDelay : if true, will exit if 255 loops
// Output: 
//		LoopI = K7Read count (limited to 254 if LimitDelay==0) ; or Error (-EndOfFileErr / -WavInReadErr)
//		Glob_CpuTime updatedDelay since function entry leading signal level change ;

int16_t LevelChangeLoops(int16_t ExpectedTtlTrigger, uint16_t OffsetDelay, uint16_t LoopDelay, bool LimitDelay, bool IntEnabled)
{
	double WavInSampleI_Debug;
	int16_t WavSignal;
	int16_t TTLSignal; //Inverted if WavIn_InvertSignal==1
	uint32_t WavInPosNew;
	int16_t LoopI = 0;
	bool NotTriggered;
	uint16_t InterruptDelay = 0;

	WavSignal = 0;
	Glob_CpuTime = Glob_CpuTime + OffsetDelay;
	do
	{
		if ((IntEnabled)&&(AllowInterruptSimul))
		{
			InterruptDelay = InterruptSimul_Delay(0);
			Glob_CpuTime+= InterruptDelay;
		}

		WavInPosNew = (uint32_t)(Glob_CpuTime * CurrentWavIn.Head.SampleRate / CpuFq * CurrentWavIn.Head.BlockAlign + WavInPosOffset);

		WavInSampleI_Debug = ((double)Glob_CpuTime) * CurrentWavIn.Head.SampleRate / CpuFq;
		if (WavInPosNew > WavInPosMax)
		{
			return (-EndOfFileErr);
		}
		if (fseek(WavInFile, WavInPosNew, SEEK_SET))
		{
			return (-WavInReadErr);
		}
		if (fread(&WavSignal, CurrentWavIn.SampleLen, 1, WavInFile) != 1)
		{
			return (-WavInReadErr);
		}
		if (CurrentWavIn.SampleLen != 1) // 2 bytes from -32768 to 32767
		{
			WavSignal = WavSignal / 256 + 128; // Back to 0-255
		}
		TTLSignal = (WavIn_InvertSignal == 0 ? WavSignal : 255 - WavSignal);
		NotTriggered = (((ExpectedTtlTrigger >= 128) && (TTLSignal < ExpectedTtlTrigger)) || ((ExpectedTtlTrigger < 128) && (TTLSignal > ExpectedTtlTrigger)));
		if ((LoopI < 254) || (LimitDelay))
		{
			LoopI++; // Limited not to have a negative value
		}

#if(WavIn_Display_Debug==1) // For debug
		printf("CpT=%06d,Trg=%02d,TtlV=%03d,NS=%06d,FPs=%02d,Ofs=%03d,", 
			(uint32_t)Glob_CpuTime, (uint8_t)!NotTriggered, (uint8_t)TTLSignal, (uint32_t)(Glob_CpuTime * CurrentWavIn.Head.SampleRate / CpuFq), (uint8_t)Glob_PosInFile, OffsetDelay);
		if ((TTLSignal < TTLNormInLevels[0])&&(InterruptDelay == 0))
		{
			printf("L0I=%03d\n", LoopI);
		}
		else if ((TTLSignal > TTLNormInLevels[1]) && (InterruptDelay == 0))
		{
			printf("L1I=%03d\n", LoopI);
		}
		else
		{
			printf("LXI=%03d\n", LoopI);
		}
#endif

		if (NotTriggered) // For next Read (not on exit)
		{
			Glob_CpuTime += LoopDelay;
		}

		if ((LimitDelay) && (LoopI == 255)) // Exit if too many loops
		{
			NotTriggered = 0;
		}
	} while (NotTriggered);

	return (LoopI);
}


//-------------------------------------------------------------------------
// InterruptSimul_Delay 
//-------------------------------------------------------------------------
// Input : MinDelay, delay to be added to Glob_CpuTime (o/w if no interrupt)
// Output : Delay to be added to Glob_CpuTime due to interrupts + MinDelay
uint16_t InterruptSimul_Delay(int16_t MinDelay)
{
	#if(AllowInterruptSimul==1)
		uint64_t EnabledCpuT=0;
		uint16_t Delay;
		if (Glob_CpuTime > 15) { EnabledCpuT = Glob_CpuTime - 15; } //  to EI
		if ((Rst7Period_Delay + Rst7_LastCpuTime) > (Rst6Period_Delay + Rst6_LastCpuTime)) // XXX is it the right test ?
		{
			Delay = Rst6Simul_Delay(EnabledCpuT, MinDelay) ;
			Delay = Rst7Simul_Delay(EnabledCpuT, MinDelay + Delay) + Delay;
		}
		else
		{
			Delay = Rst7Simul_Delay(EnabledCpuT, MinDelay);
			Delay = Rst6Simul_Delay(EnabledCpuT, MinDelay + Delay) + Delay;
		}
		return (Delay + MinDelay);
	#else
		return (MinDelay);
	#endif

}


//-------------------------------------------------------------------------
// Rst6Simul_Delay 
//-------------------------------------------------------------------------
// Input :	EnabledCpuTime, time at which interrupts have been enabled
//			EnabledPeriod, delay after EnabledCpuTime during which interrupts remain enabled
// Output : Delay to be added to Glob_CpuTime if interrupt Rst 6 is triggered, 0 if no trigger
uint16_t Rst6Simul_Delay(uint64_t EnabledCpuTime, uint16_t EnabledPeriod)
{
	uint16_t Delay ;
	if (EnabledCpuTime > (Rst6Period_Delay + Rst6_LastCpuTime))
	{
		if (Rst6_NextDelayIsShort)
		{
			Rst6_NextDelayIsShort = false;
			Delay = (Rst6A_Clock_Delay);
		}
		else
		{
			Rst6_NextDelayIsShort = true;
			Delay = (Rst6B_Clock_Delay);
		}
		Rst6_LastCpuTime = EnabledCpuTime;
		#if(WavIn_Display_Interrupt==1)
			printf("Rst6____________________,CpuT=%06d,SplI=%06d,EnabledT=%06d,AddedTime=%04d \n", (uint32_t)Glob_CpuTime, (uint32_t)(Glob_CpuTime * CurrentWavIn.Head.SampleRate / CpuFq),(uint32_t)Rst6_LastCpuTime, Delay);
		#endif
		return (Delay);
	}
	return (0);
}


//-------------------------------------------------------------------------
// Rst7Simul_Delay 
//-------------------------------------------------------------------------
// Input :	EnabledCpuTime, time at which interrupts have been enabled
//			EnabledPeriod, delay after EnabledCpuTime during which interrupts remain enabled
// Output : Delay to be added to Glob_CpuTime if interrupt Rst 7 is triggered, 0 if no trigger
uint16_t Rst7Simul_Delay(uint64_t EnabledCpuTime, uint16_t EnabledPeriod)
{
	if (EnabledCpuTime > (Rst7Period_Delay + Rst7_LastCpuTime))
	{
		Rst7_LastCpuTime = EnabledCpuTime;
		#if(WavIn_Display_Interrupt==1)
		printf("Rst7____________________,CpuT=%06d,SplI=%06d,EnabledT=%06d,AddedTime=%04d \n", (uint32_t)Glob_CpuTime, (uint32_t)(Glob_CpuTime * CurrentWavIn.Head.SampleRate / CpuFq), (uint32_t)Rst6_LastCpuTime, Rst7_Clock_Delay);
#endif
		return (Rst7_Clock_Delay);
	}
	return (0);
}

//-------------------------------------------------------------------------
// ReadLeader 
//-------------------------------------------------------------------------
// Read samples until start of Sync Byte
// ReadLeader + Sync Bit
// No interrupt (keyboard scan / cursor blink) is modelized
// Resets Glob_CpuTime to the beigining of the first High level found after low level
int16_t ReadLeader(void)
{
	#define HighLevelLoopsEstimation 0x28 // Number of High level loopson ReadLeader entry, set in firmware  

	uint64_t FirstHigh_Time;
	uint16_t InterDelay;
	int16_t TriggerHigh;
	int16_t TriggerLow;
	uint8_t PulseNToSync ;
	bool OutOfMargin;
#if(WavIn_Display_Debug!=0)
	LoopHOld_Debug=0;
#endif

	TriggerLow = TTLNormInLevels[0] ;
	TriggerHigh = TTLNormInLevels[1] ;
	LoopH = HighLevelLoopsEstimation;
	InterDelay = 0 ; 

FwDai_RDL05: // 0xD488, See DAI Firmware labels 
	FirstHigh_Time = 0;
#if(WavIn_Display_Debug!=0)
	LoopHOld_Debug = LoopHOld;
#endif
	LoopHOld = LoopH;
	InterDelay += 7;
//FwDai_RDL10: // 0xD48A
	InterDelay += 19; // 19 from start of RDL05 to first K7 Read (included)
	// Interrupt may add cycles to blink cursor or check keyboard (ex: Rst7_Clock_Delay, Rst6A_Clock_Delay, Rst6B_Clock_Delay)
#if(WavIn_Display_Debug==2) // For debug
	if (LoopH == HighLevelLoopsEstimation)
	{ printf("Leader___Start_FirstRead,"); }
	else 
	{ printf("Leader_Restart_FirstRead,"); }
	printf("CpuT=%06d,SplI=%06d,Loop_1=%04d,Loop_0=%04d \n", 
		(uint32_t)Glob_CpuTime, (uint32_t)(Glob_CpuTime * CurrentWavIn.Head.SampleRate / CpuFq), LoopHOld_Debug, LoopH);
#endif
	LoopH = LevelChangeLoops(TriggerLow, InterDelay, 29, false, true); if (LoopH < 0) return (LoopH);
	InterDelay = 22; // After First low read to RDL30
	PulseNToSync = LeaderMinHighLevelsForSync ;
	OutOfMargin = false;

FwDai_RDL30: // 0xD494
	// Wait for High (DCR E) -> Low length
	InterDelay += 33; // From start of RDL30 to first Ora M included
	LoopL = LevelChangeLoops(TriggerHigh, InterDelay, TailsCyclesPerLoop, true, false); if (LoopL < 0) return (LoopL);
	if (LoopL >= 255)
	{
		InterDelay = 25; // If restart 
		goto FwDai_RDL05; // This is not fully comparable to firmware
	}
	InterDelay = 17; // After First High to RDL50
//FwDai_RDL50: // 0xD4A1
	InterDelay += 22;  // From start of RDL50 to first Ana M included
	if (FirstHigh_Time == 0) { FirstHigh_Time = Glob_CpuTime; }
	// Wait for Low (INR B) -> High length
	LoopH = LevelChangeLoops(TriggerLow, InterDelay, TailsCyclesPerLoop, true,false); if (LoopH < 0) return (LoopH);
	if (LoopH >= 255)
	{
		InterDelay = 25; // If restart 
		goto FwDai_RDL05; // This is not fully comparable to firmware
	}
	InterDelay = 72 + (LoopHOld > LoopH ? 9 : 0);  // From last read (excluded) to End of test included (0xD4B8) 
	// Margin criteria makes sens only when LoopH >= 16
	// Depending of CPU / WAV synchonisation, LoopH varies from 16 to 20, observed delta up to 3 for Mame created 44k1 waves

	OutOfMargin = false;
	if (abs(LoopH - LoopHOld) > ((LoopHOld & 0xF0) >> 3)) // RDL60, 0xD4B0
	{
		OutOfMargin = true;
	}
	if (OutOfMargin) // RDL60, 0xD4B0
	{
		goto FwDai_RDL70;
	}

	// If sync achieved: 0xD4B8
	PulseNToSync--;
	if (PulseNToSync != 0)
	{
		InterDelay += 15; 
		goto FwDai_RDL30; // Delay between K7 read: 120 + (LoopHOld < LoopH ? 9 : 0)
	}

	PulseNToSync++;
	InterDelay += 30;
	goto FwDai_RDL30;  // Delay between K7 read: 135 + (LoopHOld < LoopH ? 9 : 0)

FwDai_RDL70: // 0xD4C3, If out of margin: 
	PulseNToSync--;
	if (PulseNToSync != 0)
	{
		InterDelay += 15; // Delay between K7 read: 113 + (LoopHOld < LoopH ? 9 : 0)
		goto FwDai_RDL05;
	}

	// Wait high, FwDai_RDL80, test xD4C8
	InterDelay += 26; // Delay between K7 read: 98 + (LoopHOld < LoopH ? 9 : 0)
	LoopL = LevelChangeLoops(TriggerHigh, InterDelay, 17, false, false); if (LoopL < 0) return (LoopL);
	InterDelay = 17; // Delay between K7 read: 17
	// Wait low, FwDai_RDL90, test xD4CC
	LoopH = LevelChangeLoops(TriggerLow, InterDelay, 17, false, false); if (LoopH < 0) return (LoopH);
	// Delay to next K7Read, InterDelay = 175 
#if(WavIn_Display_Debug > 1) // For debug
	printf("Leader___SyncBit Exit___,"); 
	printf("CpuT=%06d,SplI=%06d,Loop_1=%04d,Loop_0=%04d,", (uint32_t)Glob_CpuTime, (uint32_t)(Glob_CpuTime * CurrentWavIn.Head.SampleRate / CpuFq), LoopH, LoopHOld_Debug);
#endif
	return (0);
}


//-------------------------------------------------------------------------
// ReadDaiCore 
//-------------------------------------------------------------------------
// Read all information to a bin structure excluding Leader and Trailer & Sync Byte
// Input: WavOutFile and global variables 
// Output : 0 or negative error code
int16_t ReadDaiCore(void)
{
	uint16_t DataI;
	uint8_t DataCS;
	int16_t DaiByte;
	int16_t NErr = 0;

	Glob_PosInFile = PosInFile_ProgByte; 
	DaiByte = ReadDaiByte();
	Glob_BinByteI_Debug = 0; // Starts at 0
	if (DaiByte < 0) { return (DaiByte); }
	Glob_ProgType = (uint8_t) DaiByte;
	if (Glob_ProgType>0x32) { return (-WavInProgTypeErr); }

	for (Glob_BlockI = 0; Glob_BlockI < DataBlock_Count; Glob_BlockI++)
	{
		Glob_PosInFile = PosInFile_InBlock;

		// Read Len and checksum of len
		Glob_PosInBlock = PosInBlock_LenH;
		DaiByte = ReadDaiByte(); if (DaiByte < 0) { return (-WavInBlockLenErr); }
		DaiBlocksInfo[Glob_BlockI].Len = DaiByte<<8;
		Glob_PosInBlock = PosInBlock_LenL;
		DaiByte = ReadDaiByte(); if (DaiByte < 0) { return (-WavInBlockLenErr); }
		DaiBlocksInfo[Glob_BlockI].Len += DaiByte ;
		if (DaiBlocksInfo[Glob_BlockI].Len>0xFFFF) { return (-WavInBlockLenErr); }
		Glob_PosInBlock = PosInBlock_LenCS;
		DaiByte = ReadDaiByte(); if (DaiByte < 0) { return (-WavInBlockLenCSErr); }
		DaiBlocksInfo[Glob_BlockI].LenCS = (uint8_t) DaiByte;

		// Len Checksum
		if(DaiBlocksInfo[Glob_BlockI].LenCS!=DaiWordCheckSum(DaiBlocksInfo[Glob_BlockI].Len)) { return (-WavInBlockLenCSErr); }
		Glob_PosInBlock = PosInBlock_InData;

		// Create Block arrays
		DaiBlocksInfo[Glob_BlockI].Block = (char*)calloc(DaiBlocksInfo[Glob_BlockI].Len, 1);
		if (DaiBlocksInfo[Glob_BlockI].Block == NULL)  return (-WavInCallocErr);
		DataCS = 0x56;
		// Read data part of block
		for (DataI = 0; DataI < DaiBlocksInfo[Glob_BlockI].Len; DataI++)
		{
			if (DataI > 0)
			{
				Glob_PosInBlock = PosInBlock_InData;
			}
			else
				if (DataI == (DaiBlocksInfo[Glob_BlockI].Len - 1))
				{
					Glob_PosInBlock = PosInBlock_LastByte;
				}
			DaiByte = ReadDaiByte(); if (DaiByte < 0) { return (-WavReadBlockErr); }
			DaiBlocksInfo[Glob_BlockI].Block[DataI] = (uint8_t) DaiByte;
			DataCS = DaiByteCheckSum((uint8_t) DaiByte, DataCS);
		}

		if (Glob_BlockI == 0)
		{
			Glob_PosInFile = PosInFile_BlockCS0;
		}
		else
		{
			Glob_PosInFile = PosInFile_BlockCSN;
		}
		DaiByte = ReadDaiByte(); if (DaiByte < 0) { return (-WavInBlockCSErr); }
		DaiBlocksInfo[Glob_BlockI].BlockCS = (uint8_t) DaiByte;
		// Block Checksum
		if (DataCS != DaiBlocksInfo[Glob_BlockI].BlockCS) { return (-WavInBlockCSErr); }
	}
	return(0);
}


//-------------------------------------------------------------------------
// ReadDaiByte 
//-------------------------------------------------------------------------
// Convert a byte in wave samples
// Input:
// - Byte to write as a DaiBit
int16_t ReadDaiByte(void)
{
	uint8_t BitMask ;
	int16_t DaiBit ;
	int16_t DataByte ;

	DataByte = 0 ;
	for (BitMask = 0x80; BitMask != 0; BitMask = BitMask >> 1)
	{
		DaiBit = ReadDaiBit(Glob_InterK7ReadDelay); 
		if (DaiBit > 0)
		{
			DataByte += BitMask;
		}
		else if (DaiBit < 0) // Error
		{
			return (DaiBit);
		}
		// Delay between reading bits, if not last bit
		Glob_InterK7ReadDelay = ExitDaiBit_Delay + InterDaitBits_Delay + EnterDaiBit_Delay;
	}

	// Calculate Glob_InterK7ReadDelay : delays between last read Sample and next one for writing next byte
	if (Glob_PosInFile != PosInFile_InBlock) // First one to use it is Glob_PosInFile == PosInFile_SyncByte
	{	// Delay between bytes when not in Block
		Glob_InterK7ReadDelay = ExitDaiBit_Delay + EnterDaiBit_Delay + OutBkInterCallsDelays[Glob_ProgType - 0x30][Glob_PosInFile];
		Glob_InterK7ReadDelay += OutBkInterCallsDelaysMargin[Glob_PosInFile];
	}
	else
	{	// Delay between trying to read last sample of a byte and 1st sample of a byte
		Glob_InterK7ReadDelay = ExitDaiBit_Delay + InBkInterCallsDelays[Glob_BlockI][Glob_PosInBlock] + EnterDaiBit_Delay;
		Glob_InterK7ReadDelay += InBkInterCallsDelaysMargin[Glob_PosInBlock];
	}
	Glob_BinByteI_Debug += 1;
	return (DataByte);
}


//-------------------------------------------------------------------------
// ReadDaiBit 
//-------------------------------------------------------------------------
// Convert a DaiBit, which has 4 periods from 0 to 3, into a bit
// Output DaiBit level (0=Low, 1=High) or -1 if error or -2 end of file
int16_t ReadDaiBit(uint16_t InterCallsK7ReadDelay)
{
	uint8_t DaiBitPeriod;
	uint16_t RequiredPeriodMinDelay;
	uint16_t LoopDelay;
	int16_t LoopsN[4];
	int16_t ExpectedTtlTrigger;

	for (DaiBitPeriod = 0; DaiBitPeriod < 4; DaiBitPeriod++)
	{
		// Loop related part. 1 loop is already included in InterCallsK7ReadDelay
		// Header / Trailer timing is approximative
		LoopDelay = DaiBitCyclesPerLoop[DaiBitPeriod];
		RequiredPeriodMinDelay = ((DaiBitPeriod == 0) ? InterCallsK7ReadDelay : LoopDelay) + (DaiBitPeriod == 2 ? 10 : 0);
		ExpectedTtlTrigger = TTLNormInLevels[1-(DaiBitPeriod & 0x01)] ;
		LoopsN[DaiBitPeriod] = LevelChangeLoops(ExpectedTtlTrigger, RequiredPeriodMinDelay, LoopDelay, true, false);
		if (LoopsN[DaiBitPeriod] < 0)
		{
			return (LoopsN[DaiBitPeriod]);
		}
	}
	if (LoopsN[1] > LoopsN[3])
	{
		return (1);
	}
	return (0);
}


//-------------------------------------------------------------------------
// DgvWavIn
//-------------------------------------------------------------------------
// Main function to read wav file 
int16_t DgvWavIn(char* FileName, bool WavInParity)
{
	int16_t NErr;
	int16_t SyncByte = 0;
	int16_t DaiByte = 0;
	uint16_t CpuTimeStart = 0 ;
	uint32_t SampleIOnByteSyncStart_Debug = 0 ;

	WavIn_InvertSignal = WavInParity;

	ReadWavHeader(FileName, &CurrentWavIn);
	WavInPosOffset = CurrentWavIn.DataPos + CurrentWavIn.SampleLen * (WavInChannel); // At Glob_CpuTime, WavInPos = End of Wav Header
	WavInPosMax = CurrentWavIn.DataPos + (CurrentWavIn.SamplesPerChannel) * CurrentWavIn.Head.BlockAlign - CurrentWavIn.SampleLen; // Last begining of last sample


	if (CurrentWavIn.SampleLen > 2)
	{
		NErr = WavInHeaderErr;
		goto ExitDgvWavIn;
	}

	// Reopen file
	WavInFile = fopen(FileName, "rb");
	if (WavInFile == NULL)
	{
		printf("Could not open %s \nPress Enter to exit\n", FileName);
		NErr = WavInHeaderErr;
		goto ExitDgvWavIn;
	}

	// Test different delay between Cpu clok and Wav clock
	for (CpuTimeStart = CpuTimeStartOffset_VariationMin; CpuTimeStart <= CpuTimeStartOffset_VariationMax; CpuTimeStart++)
	{
		// Real Leader, Sync Bit, Sync Byte
		SyncByte = 0; // For debug
		Glob_CpuTime = CpuTimeStartOffset + CpuTimeStart; 
		Glob_BinByteI_Debug = 0;
		Rst6_LastCpuTime = Init_Rst6_CpuTime; 
		Rst6_NextDelayIsShort = false; 
		Rst7_LastCpuTime = Init_Rst7_CpuTime; 	

	SearchSyncByte:
		ClearDaiBinInfos();
		Glob_PosInFile = PosInFile_Leader;
		Glob_ProgType = 0x30; // Necessary to get Glob_InterK7ReadDelay at the end of PosInFile_SyncByte
		NErr = ReadLeader(); 
		if (NErr < 0) 
		{	
			goto ExitDgvWavIn;
		}

		Glob_InterK7ReadDelay = SyncBitExit_Delay + SyncBitDaiBit_Delay + EnterDaiBit_Delay;
		Glob_PosInFile = PosInFile_SyncByte;
		SampleIOnByteSyncStart_Debug = (uint32_t)((Glob_CpuTime + Glob_InterK7ReadDelay) * CurrentWavIn.Head.SampleRate / CpuFq) ;
		SyncByte = ReadDaiByte(); // Result should be 0x55
#if(WavIn_Display_Debug > 1) // For debug
		printf("SyncByte=%04d\n", SyncByte);
#endif
		if (SyncByte < 0)
		{ 
			NErr = -WavWriteBlockErr;
			goto ExitDgvWavIn;
		}
		if (SyncByte != 0x55) // Due saved files
		{ 
			#if(RestartIfInvalidSyncByte) // This is the case in firmware
				Glob_CpuTime +=  InterruptSimul_Delay(367); // Delay due to calling Disable sound interrupt and restart of Read Leader
				goto SearchSyncByte;
			#else
				NErr = -WavInSyncTypeErr;
			#endif
			goto ExitDgvWavIn;
		}

		for (Glob_BlockI = 0; Glob_BlockI < DataBlock_Count; Glob_BlockI++)
		{
			DaiBlocksInfo[Glob_BlockI].Block = NULL;
		}

		// Read Program / Variables information, starting by Type byte
		NErr = ReadDaiCore();
	ExitDgvWavIn:
#if(WavIn_Display_Debug == 3)
		printf("CpuTimeStart=%03d, Err=%04d, CpuTExit=%06d, SByteSyncStart=%04d, SyncByte=%03d\n", CpuTimeStart, NErr, (uint32_t)Glob_CpuTime, SampleIOnByteSyncStart_Debug, SyncByte);
		printf("===============================================================================\n");
		printf("\n");
#endif
		NErr = NErr;
	}

	if (WavInFile != NULL) { fclose(WavInFile); }
	return (NErr);
}

