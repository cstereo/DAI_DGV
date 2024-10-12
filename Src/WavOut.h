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

#ifndef WAVOUT_H
#define WAVOUT_H
#include <stdint.h> 
#include "Const.h"
#include "WavIO.h"

//-------------------------------------------------------------------------
// USER Definitions
//-------------------------------------------------------------------------
// For Mame, only 2 levels are necessary. Not 0 and 255, because of a bug in old Mame version
// For a DAI, Levels transition can be limited to have less transient issues
// USER CHOICE

// Margin to improve reliability (normally only for Geneting wav for a physical DAI)
#define WavOut_SmoothSignal 0 // Smooth signal to limit transition spikes, requires input signal to be higher

//-------------------------------------------------------------------------
// Global constants 
//-------------------------------------------------------------------------
// Extension for different types of hardware, see PrintHelp for details

//---------------
// Options bit corresponding to command lines parameters that have been changes
#define OptionBit_Hardware 0x01
#define OptionBit_NChannels 0x02
#define OptionBit_NBytes 0x04
#define OptionBit_Parity 0x08
#define OptionBit_Frequency 0x10
#define OptionBit_Periods 0x20
#define OptionBit_InterDelays 0x40
#define OptionBit_OptionArgument 0x8000 // An Options argument is present. Argument can however be invalid
#define OptionBits_Users (OptionBit_Hardware|OptionBit_NChannels|OptionBit_NBytes|OptionBit_Parity)


//---------------
// DaiHardware_Struct
//---------------
// I)  ProfileName, use to add option to file names
// II) DaiBitPeriod_Loops[DaiBitType][DaiBitPeriod]
//		Loops count according to hardware, Type of Bit (Fast TTL Low/High,Slow Low/High, Leader, Trailer, SyncBit) and Period in DaiBit
//		For Leader, 2 High pulses must be equal with length margin of maximum 1/8 th -> in practice periods P1 & P3 loops count must be equal
//		TTL Low means that signal will be high in a wav for the DAI
//		For normal DaiBit, loop count should be : P3>P2+1 for TTL High & P2>P3+2 for TTL low
//		DaiBitType
//			Fast DaiBits TTL Low (Cpu cycle delay per loop = 32,37,32,37)
//			Fast DaiBits TTL High
//			Slow DaiBits TTL LoW
//			Slow DaiBits TTL High
//			Leader (Cpu cycle delay per loop for leader and trailer = 32,32,32,32 here, which is not totally similar to a DAI)
//			Trailer, must have HighLevels 'LoopI' out of margin (i.e. difference > 3 vs previous TTL High) (Loop Cpu cycle delay = 32,32,32,32)
//			SyncBit XXXX : first HighLevel LoopI must be out of margin (i.e. difference > 3 vs previous TTL High loop count)
//		
//  III) PeriodsOffset_HwDelay[DaiBitPeriod], Cpu cycles offset to theoric values for each period of a DaiBit
//			{-42,42,-42,42}, // On V4toV7 Correction for LM324 due to slew rate and limitation to positive value (diodes impact)
// 			{-35,35,-35,35}, // On V4 Correction for LM324 due to slew rate and limitation to positive value (diodes impact)
// 			{ -7, 7, -7, 7}, // On V4toV7 Correction for TLC274 due to slew rate and limitation to positive value (diodes impact)
//	IV) Default WavOut file characteristics
//			NChannels			// Number of channels in wav file, depends on playing device capabilities
//			Bytes_per_sample	// Samples size in bytes : 1 = B = 1 byte, 2 = W = Word of 2 bytes
//			InvertSignal		// 1, when playing through Dai K7 operational amplifier as signal is inversed vs TTL levels (the reference in this program)
//			SamplingFq			// Sampling frequency depending on the capabilities of the playing device, the necessary bandwidth. 1 sample = 20.8333 us at 96KHz
//			Leader_ms			// Number of DaiBits (4 periods) require for the Leader, depends also on playing Device and Dai version
//			Trailer_ms			// Number of DaiBits (4 periods) require for the Trailer. Mostly useless in not recorder on a serial Device (such as K7 player)
//								// Delays are in ms and will be actually sligthly higher due to rounding at DaiBit level
//								// Leader needs to be above 750ms if the waiting period for K7 player speed stabilization needs to be emulated 
//								// Leader must also be greater than
//										A few ms to stabilize analog signal
//										plus typically 240m to mitigate interrupt negative impact on synchronization
//										LeaderMinHighLevelsForSync/2 DaiBits
//
struct DaiHardware_Struct
{
	const char* ProfileName;
	uint8_t DaiBitPeriods_MinLoops[DaiBitType_Count][DaiBitPeriod_Count];
	int16_t PeriodsOffset_HwDelay[DaiBitPeriod_Count];  
	uint8_t NChannels;
	uint8_t Bytes_per_sample;
	uint8_t	InvertSignal;
	uint32_t SamplingFq;
	uint16_t Leader_ms;
	uint16_t Trailer_ms;
};

// DaiHwI_BitMask : List of DaiHw files to be generated (bit coded b0= HW0, ...) when not specifying hardware version 
#define DaiHwI_BitMask 0b0000000011111111
#define DaiHW_Count (sizeof(DaiHW_Profile)/sizeof(DaiHardware_Struct))
#define DaiHW_Default 7 // Starts at 0, MameA

static const struct DaiHardware_Struct DaiHW_Profile[] = {
	{	// V0 - Similar to a Dai or Mame signal output, 24KHz, 208s for Envahisseurs
		// OK on V4ToV7 with LM324 + dongle FiiO KA11 at 384KHz 2x16bit, vol 29% on PC, (~+/-300mV)
		"_DgvDaiK7", 
		{{13,16,28,25},{26,25,15,16}, {13,16,28,25},{26,25,17,16}, {17,18,17,18},{12,13,17,17},{29,29,19,20}}, // DaiBitPeriods_MinLoops
		{0,0,0,0}, // PeriodsOffset_HwDelay
		1,1,1,24000, // NChannels, Bytes_per_sample, InvertSignal, SamplingFq 
		2900,10 // Leader_ms, Trailer_ms, delays (in ms) will be actually sligthly higher due to rounding at DaiBit level
	},
	{	// V1 - Optimized for Dai V4, 48KHz, 56s for Envahisseurs
		"_DgvDaiV4", // OK on V4 with LM324, dongle FiiO KA11 at 384KHz 2x16bit, vol  59% on PC, (~+/-1.38V)
		{{5,3,5,5},{5,5,5,3}, {17,10,17,25},{17,25,17,10}, {10,17,10,17},{10,10,14,14},{10,10,10,10}},
		{-35,35,-35,35}, // Experimental changes to correction
		1,1,1,48000,
		600,10
	},
	{	// V2 - Optimized for Dai V7, 48KHz, 56s for Envahisseurs
		"_DgvDAIV7A", // OK V4ToV7 with LM324 + dongle FiiO KA11 at 384KHz 2x16bit, Envahisseurs = 55.4s, vol 31% on PC (~+/-450mV)
		{{5,3,5,5},{5,5,5,3}, {17,10,17,25},{17,25,17,10}, {10,17,10,17},{10,10,14,14},{10,10,10,10}},
		{-22,42,-22,42}, // Experimental changes to correction
		1,1,1,48000,
		600,10
	},
	{	// V3 - Optimized for Dai V7, 48KHz, 48s for Envahisseurs
		"_DgvDaiV7B", // V4ToV7 with LM324 + dongle FiiO KA11 at 384KHz 2x16bit, Envahisseurs = 48.4s, vol 31% on PC (~+/-450mV)
		{{4,3,4,4},{4,5,4,3}, {17,10,17,25},{17,25,17,10}, {10,17,10,17},{10,10,14,14},{10,10,10,10}},
		{-22,42,-22,42}, // Experimental changes to correction
		1,1,1,96000,
		600,10
	},
	{	// V4 - Optimized for Dai V7, 384KHz, 43s for Envahisseurs
		"_DgvDaiV7C", // OK on V4ToV7 with LM324 + dongle FiiO KA11 at 384KHz 2x16bit, , Envahisseurs = 43.1s, vol 31% on PC (~+/-450mV)
		{{4,3,4,4},{4,5,4,3}, {17,10,17,25},{17,25,17,10}, {10,17,10,17},{10,10,14,14},{10,10,10,10}},
		{-42,42,-42,42}, // Correction for LM324 due to slew rate and limitation to positive value (diodes impact)
		1,1,1,384000,
		600,10
	},
	{	// V5 - Optimized for modified Dai V7, 192KHz, 33s for Envahisseurs
		"_DgvDaiV7T", // OK on V4ToV7 with TLC274 + dongle FiiO KA11 at 384KHz 2x16bit, Envahisseurs = 32.8s, PC = 39% (+/- 700mV)
		{{2,2,2,3},{2,4,2,2}, {17,10,17,25},{17,25,17,10}, {10,17,10,17},{10,10,14,14},{10,10,10,10}},
		{0,14,0,14},
		1,1,1,192000,
		600,10
	},
	{	// V6 - Optimized for modified Dai V7, 384KHz, 27s for Envahisseurs
		"_DgvDaiV7U", // OK on V4ToV7 with TLC274 + dongle FiiO KA11 at 384KHz 2x16bit, PC = 39% (+/- 700mV), Envahisseurs = 26.6s
		{{1,2,1,3},{1,4,1,2}, {17,10,17,25},{17,25,17,10}, {10,17,10,17},{10,10,14,14},{10,10,10,10}},
		{0,0,0,0},
		1,1,1,384000,
		600,10
	},
	{	// V7 - Optimized for Mame, 96KHz, 22s for Envahisseurs
		"_DgvMameA", // Ok on Mame
		{{1,0,1,1},{1,2,1,0}, {3,2,3,5},{3,6,3,4}, {10,17,10,17},{10,10,14,14},{10,10,10,10}},
		{0,22,0,22}, // {32 * 0,37 * 0 + 22,32 * 0,37 * 0 + 22}
		1,1,0,96000,
		250,0
	},
	{	// V8
		"_DgvMameB", // Ok on Mame -> Envahisseurs = 23.5s
		{{1,1,1,2},{1,3,1,1}, {17,10,17,25},{17,25,17,10}, {10,17,10,17},{10,10,14,14},{10,10,10,10}},
		{0,0,0,0},
		1,1,0,96000,
		600,10
	},
	{	// V9
		"_DgvTest9", // OK on V4ToV7 with TLC274 + dongle FiiO KA11 at 384KHz 2x16bit, PC = 39% (+/- 700mV), Envahisseurs = 33.2s
		{{2,2,3,3},{2,4,3,2}, {17,10,17,25},{17,25,17,10}, {10,17,10,17},{10,10,14,14},{10,10,10,10}},
		{-7,7,-7,7},
		1,1,1,192000,
		600,10
	},
};


//---------------
// Samples levels definitions, WavOutLevels[LevelChange][TTL_level]
const uint16_t WavLevels_2B[2][2] = { {(uint16_t)-28835,28835}, {(uint16_t)-26214,26214}}; // When samples are int16_t type
const uint16_t WavLevels_1B[2][2] = { {0,255}, {50,208} }; //  {15,240} {50,208} or {26,230} // When samples are uint8_t type


//-------------------------------------------------------------------------
// Global variables 
//-------------------------------------------------------------------------
extern int16_t PeriodsOffset_Delay[DaiBitPeriod_Count];
extern int16_t InBkInterCallsDelaysMargin[PosInBlock_Count] ;
extern int16_t OutBkInterCallsDelaysMargin[PosInFile_Count] ;

extern struct WavHeader_Struct WavOutHeader;
extern struct WavSubchunk_Struct WavOutSubChunks[NChunkMax];

extern char WavOut_NameOptions[OptionsLenMax+2];

//-------------------------------------------------------------------------
// Global functions 
//-------------------------------------------------------------------------

int16_t DgvWavOut(char* WavFileName);
void Update_WavOut_NameOptions(char* Options);
uint16_t LoadProgOptionsArgument(char* Options);

#endif