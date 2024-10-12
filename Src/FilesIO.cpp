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

/***********************************************************************************
* Filename : FilesIO.cpp
***********************************************************************************/
// Create, add, wav files 
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <stdint.h> 
#include "DgvMain.h"
#include "FilesIO.h"
#include "WavOut.h"
#include "WavIn.h"


//-------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------
DaiBlock_Struct DaiBlocksInfo[3];
uint8_t Glob_ProgType;

//-------------------------------------------------------------------------
// Local variables
//-------------------------------------------------------------------------
// Reading / Writing wav structures




//-------------------------------------------------------------------------
// Local functions 
//-------------------------------------------------------------------------
// Reading Bin functions
int16_t ReadBinDataAndCheck(char* Data, uint16_t DataLen, FILE* BinFile);
int16_t ReadDaiWordAndCheck(uint16_t* Word, FILE* BinFile);


// Reading Wav functions
int16_t CheckWaveHeader(struct WavHeader_Struct WavHeader);
int16_t ReadHeaderSubChunk(FILE* WFile, Wav_Struct* CurrentWav);
int16_t ReadOlympusDate(char* pText, uint16_t* ClapTime);
int16_t ValidFileTime(uint16_t* FileTime);
int16_t Car2Num(char* TNum);
int16_t Car3Num(char* TNum);


//=========================================================================
//  WRITING WAV FUNCTIONS
//=========================================================================

//-------------------------------------------------------------------------
// CreateWavOut
//-------------------------------------------------------------------------
// Create a wav file with its header, without data, then close the file 
// ChunkSize=Format:4b+fmt chunk:24b+SubChunk2ID:4b+SubChunk2Size:4b+SubChunk2:<SubChunk2Size>=36 + <SubChunk2Size>
int16_t CreateWavOut(FILE* WaveFile, uint32_t NSamples, uint32_t SRate, uint8_t NChannels, uint8_t Bytes_per_sample)
{

	if ((WaveFile == NULL) || (SRate <= 2400)) return (-1);

	// Create header
	memcpy(&WavOutHeader.ChunkId, "RIFF", 4);
	WavOutHeader.ChunkSize = 36 + (Bytes_per_sample * NSamples); // NSamples = NSamplesPerChannel * WavOut_NChannels
	memcpy(&WavOutHeader.Format, "WAVE", 4);
	memcpy(&WavOutHeader.Subchunk1Id, "fmt ", 4);
	WavOutHeader.Subchunk1Size = 16;
	WavOutHeader.AudioFormat = 1;
	WavOutHeader.NumChannels = NChannels;
	WavOutHeader.SampleRate = SRate;
	WavOutHeader.ByteRate = SRate * Bytes_per_sample * NChannels;
	WavOutHeader.BlockAlign = Bytes_per_sample * NChannels;
	WavOutHeader.BitsPerSample = Bytes_per_sample * 8;

	memcpy(&WavOutSubChunks[0].SubchunkId, "data", 4);
	WavOutSubChunks[0].SubchunkSize = Bytes_per_sample * NSamples;

	if (fwrite(&WavOutHeader, sizeof(WavOutHeader), 1, WaveFile) < 1) { return (-WavWriteErr); } // Can't write file header
	
	if (fwrite(&WavOutSubChunks[0], sizeof(WavOutSubChunks[0]), 1, WaveFile) < 1) // Can't write input file header
	{
		fclose(WaveFile);
		return (-WavWriteErr);
	}

	return (0);
}


//-------------------------------------------------------------------------
// UdpdateWavSize 
//-------------------------------------------------------------------------
// Update sample count of an already opened wave file
int16_t UdpdateWavSize(FILE* WaveFile, uint32_t NSamples, uint8_t Bytes_per_sample)
{
	uint32_t Size;
	uint16_t Position;
	Position = 4 ;
	Size = 36 + (Bytes_per_sample * NSamples);

	if ((fseek(WaveFile, Position, SEEK_SET)!=0)||(fwrite(&Size, sizeof(Size), 1, WaveFile) < 1)) { return (-WavWriteErr); }

	Size = Bytes_per_sample * NSamples;
	Position = (uint16_t) (sizeof(WavHeader_Struct) + sizeof(WavSubchunk_Struct) - 4);

	fseek(WaveFile, Position, SEEK_SET);
	if ((fseek(WaveFile, Position, SEEK_SET) != 0) || (fwrite(&Size, sizeof(Size), 1, WaveFile) < 1)) { return (-WavWriteErr); }

	return (0);
}

//=========================================================================
//  READING BIN FUNCTIONS
//=========================================================================


//-------------------------------------------------------------------------
// ReadDaiFile 
//-------------------------------------------------------------------------
// Read information of a ".dai" file except program data
int16_t ReadDaiFile(char* DaiFileName)
{
	uint8_t BkI;
	int16_t ChkSum;
	uint16_t DataLen;
	FILE* DaiFile;

	DaiFile = fopen(DaiFileName, "rb");
	if (DaiFile == NULL) { return(-DaiFileOpenErr); }

	for (BkI = 0; BkI < DataBlock_Count; BkI++)
	{
		DaiBlocksInfo[BkI].Block = NULL;
	}
	if (fread((char*)&Glob_ProgType, 1, 1, DaiFile) != 1)
	{
		fclose(DaiFile);
		return (-ReadDaiFileErr);
	}
	if ((Glob_ProgType < 0x30) || (Glob_ProgType > 0x32)) 
	{
		fclose(DaiFile);
		return (-ReadDaiFileErr);
	}

	for (BkI = 0; BkI < DataBlock_Count; BkI++)
	{
		ChkSum = ReadDaiWordAndCheck(&DataLen, DaiFile); 
		if (ChkSum < 0)  // Read Length of data to read
		{
			goto ExitReadDai;
		}
			
		DaiBlocksInfo[BkI].Len = DataLen;
		DaiBlocksInfo[BkI].LenCS = (uint8_t) ChkSum;

		if (((BkI == 0) && (DataLen >= 256)))
		{
			goto ExitReadDai;
		}
		if (DataLen != 0)
		{
			DaiBlocksInfo[BkI].Block = (char*)calloc(DataLen,1);
			if (DaiBlocksInfo[BkI].Block == NULL) 
			{
				goto ExitReadDai;
			}
			ChkSum = ReadBinDataAndCheck(DaiBlocksInfo[BkI].Block, DataLen, DaiFile); if (ChkSum < 0)
			{
				goto ExitReadDai;
			}

		}
		else // Some Blocks may have a len of Zero (ex no name)
		{
			if (fread((char*)&ChkSum, 1, 1, DaiFile) != 1) // Read Check sum of nothing = 0x56
			{
				goto ExitReadDai;
			}
			if (ChkSum!=0x56) 
			{
				goto ExitReadDai;
			}
		}
		DaiBlocksInfo[BkI].BlockCS = (uint8_t) ChkSum;
	}
	fclose(DaiFile);
	return(0);

ExitReadDai:
	FreeDaiBlockBuffers();
	fclose(DaiFile);
	return(-ReadDaiDataErr);
}


//-------------------------------------------------------------------------
// WriteDaiFile 
//-------------------------------------------------------------------------
// Read information of a ".dai" file except program data
int16_t WriteDaiFile(char * DaiFileName)
{
	uint8_t BkI;
	uint16_t SwappedU16;
	FILE* DaiFile;
	if ((DaiBlocksInfo[0].Block == NULL) || (DaiBlocksInfo[1].Block == NULL) || (DaiBlocksInfo[2].Block == NULL)) { return (InvalidDaiDataErr); }

	DaiFile = fopen(DaiFileName, "wb");
	if (DaiFile == NULL) { return(-WriteDaiDataErr); }
	if (fwrite(&Glob_ProgType, sizeof(Glob_ProgType), 1, DaiFile) < 1) { fclose(DaiFile); return (-WavWriteErr); }
	for (BkI = 0; BkI < 3; BkI++)
	{
		SwappedU16 = SwapBytes(DaiBlocksInfo[BkI].Len);
		if (fwrite(&SwappedU16, sizeof(SwappedU16), 1, DaiFile) < 1) { fclose(DaiFile); return (-WavWriteErr); }
		if (fwrite(&DaiBlocksInfo[BkI].LenCS, sizeof(DaiBlocksInfo[BkI].LenCS), 1, DaiFile) < 1) { fclose(DaiFile); return (-WavWriteErr); }
		if (fwrite(DaiBlocksInfo[BkI].Block, 1, DaiBlocksInfo[BkI].Len, DaiFile) < 1) { fclose(DaiFile); return (-WavWriteErr); }
		if (fwrite(&DaiBlocksInfo[BkI].BlockCS, sizeof(DaiBlocksInfo[BkI].BlockCS), 1, DaiFile) < 1) { fclose(DaiFile); return (-WavWriteErr); }
	}
	fclose(DaiFile);
	return(0);
}

//-------------------------------------------------------------------------
// FreeDaiBlockBuffers
//-------------------------------------------------------------------------
// Release memory used when reading BinFile
void FreeDaiBlockBuffers() {
	uint8_t BkI;
	for (BkI = 0; BkI < 3; BkI++)
	{
		if (DaiBlocksInfo[BkI].Block != NULL)
		{
			free(DaiBlocksInfo[BkI].Block);
			DaiBlocksInfo[BkI].Block = NULL; // XXXX
		}
	}
}

//=========================================================================
//  DAI SPECIFIC FUNCTIONS
//=========================================================================

//-------------------------------------------------------------------------
// FileReadAndCheck : read data and calculate check sum. Negative if error
//-------------------------------------------------------------------------
int16_t ReadBinDataAndCheck(char * Data, uint16_t DataLen, FILE* BinFile)
{
	uint8_t ChkSum = 0x56; // Initialize checksum algorithm
	uint8_t ChkSumRead;
	if (Data == NULL) return (-ReadDaiFileErr);

	while (DataLen != 0)
	{
		if (fread(Data, 1, 1, BinFile) != 1) return (-ReadDaiFileErr);
		ChkSum = DaiByteCheckSum((uint8_t)(*Data), ChkSum);
		Data++;
		DataLen--;
	}

	if (fread((char*) &ChkSumRead, 1, 1, BinFile) != 1) return (-ReadDaiFileErr);
	if (ChkSumRead!= ChkSum) return (-DaiDataCheckSumErr);

	return(ChkSum);
}


//-------------------------------------------------------------------------
// FileReadWordAndCheck 
//-------------------------------------------------------------------------
// Reads a word and its DAI checksum , update a word of 2 bytes, and checksum (negative if error)
int16_t ReadDaiWordAndCheck(uint16_t * Word, FILE* BinFile)
{
	uint8_t ChkSum;

	if (fread((char*)Word , 1, 2, BinFile) != 2) return (-ReadDaiDataErr);
	*Word = SwapBytes(*Word);
	if (fread((char*)&ChkSum, 1, 1, BinFile) < 1) return (-ReadDaiDataErr);
	if (DaiWordCheckSum(*Word)!= ChkSum) return (-ReadDaiDataErr);

	return(ChkSum);
}


//=========================================================================
//  READING WAV FUNCTIONS
//=========================================================================

//-------------------------------------------------------------------------
// ReadHeader
//-------------------------------------------------------------------------
int16_t ReadWavHeader(char* FileName, Wav_Struct* CurrentWav)
{
	FILE* WaveFile;
	int32_t NRead;

	WaveFile = fopen(FileName, "rb");

	if (!WaveFile)
	{
		return (-1);
	}

	fseek(WaveFile, 0, SEEK_SET); // Normally useless 

	NRead = (int32_t)fread((WavHeader_Struct*)(&CurrentWav->Head), sizeof(WavHeader_Struct), 1, WaveFile);
	if (NRead < 1) // Can't read input file header
	{
		fclose(WaveFile);
		return (-2);
	}
	if (CheckWaveHeader(CurrentWav->Head) < 0)
	{
		fclose(WaveFile);
		return (-3);
	}

	// Read  SubChunks until data is found
	if (ReadHeaderSubChunk(WaveFile, CurrentWav) < 0)
	{
		fclose(WaveFile);
		return (-4);
	}

	fclose(WaveFile);
	return (1);

}


//-------------------------------------------------------------------------
// ReadHeaderChunk : read SubChunk with data, except if data SubChunk
//-------------------------------------------------------------------------
// Input : already open file pointer, Wav structure (which includes wav header + other infos)
// Output 
//		Error number, does not close file
//		SubChunk count, last one being data, -1 if error of data chunk not found
//		WavDataPos : is the position of the first data available
int16_t ReadHeaderSubChunk(FILE* WFile, Wav_Struct* CurrentWav)
{
	uint32_t DataSize;
	int16_t LastDataChunkFound = 0;
	uint32_t WavDataPos;
	struct WavSubchunk_Struct WavInSubChunks[NChunkMax]; // Table of pointers to CurWavInSubChunksData arrays
	char* CurWavInSubChunksData[NChunkMax];

	int16_t SubChunkI = 0;
	memset(WavInSubChunks, 0, sizeof(WavInSubChunks));
	memset(CurrentWav->Time, 0, sizeof(CurrentWav->Time));

	CurrentWav->NSub = 0;
	WavDataPos = 0;
	do
	{ // Read Id and size
		if (fread(&WavInSubChunks[SubChunkI], sizeof(WavInSubChunks[0]), 1, WFile) < 1) // Can't read input file header
		{
			return (-2);
		}
		WavDataPos += 8;
		if (memcmp(WavInSubChunks[SubChunkI].SubchunkId, "data ", 4) != 0) // ==0 means equal ; Not data
		{
			DataSize = WavInSubChunks[SubChunkI].SubchunkSize; // excludes Id and Size
			CurWavInSubChunksData[SubChunkI] = NULL;
			CurWavInSubChunksData[SubChunkI] = (char*)malloc(DataSize);
			if (CurWavInSubChunksData[SubChunkI] == NULL)
			{
				return (-4);
			}
			else
			{
				if (fread(CurWavInSubChunksData[SubChunkI], DataSize, 1, WFile) < 1) // Can't read input file header
				{
					WavDataPos += DataSize;
					free(CurWavInSubChunksData[SubChunkI]);
					return (-5);
				}
			}
			if (memcmp(WavInSubChunks[SubChunkI].SubchunkId, "olym", 4) == 0) // Not data
			{
				ReadOlympusDate(CurWavInSubChunksData[SubChunkI] + 38, (CurrentWav->Time));
			}
			free(CurWavInSubChunksData[SubChunkI]);
		}
		else // Data SubChunk found
		{
			LastDataChunkFound = 1;
		}
		SubChunkI++;
	} while ((LastDataChunkFound == 0) && (SubChunkI <= NChunkMax));

	WavDataPos = 36 + 8 * SubChunkI;
	for (int16_t SB = 0; SB < SubChunkI - 1; SB++)
	{
		WavDataPos += WavInSubChunks[SB].SubchunkSize; // Add data of all subchung data size, except for the "data" subchunk
	}
	CurrentWav->DataPos = WavDataPos;
	CurrentWav->NSub = SubChunkI;

	CurrentWav->BytesPerAcquisition = WavInSubChunks[SubChunkI - 1].SubchunkSize; // Only one acquisition per .wav
	CurrentWav->SampleLen = (CurrentWav->Head.BitsPerSample) / 8;
	CurrentWav->SamplesPerChannel = CurrentWav->BytesPerAcquisition / (CurrentWav->SampleLen * CurrentWav->Head.NumChannels);	// Get the number of samples in one channel
	return (1);
}


//-------------------------------------------------------------------------
// CheckWaveHeader : // Check if a wav header is correct
//-------------------------------------------------------------------------
// return a negatif integer if error
int16_t CheckWaveHeader(struct WavHeader_Struct WavHeader)
{
	// not a little endian wav file ?
	if (memcmp(WavHeader.ChunkId, "RIFF", 4) != 0) { return ((int16_t) - __LINE__); }
	if (memcmp(WavHeader.Format, "WAVE", 4) != 0) { return ((int16_t)-__LINE__); }
	if (memcmp(WavHeader.Subchunk1Id, "fmt ", 4) != 0) { return ((int16_t)-__LINE__); }
	if (WavHeader.NumChannels == 0) { return ((int16_t)-__LINE__); }

	return (1);
}


//-------------------------------------------------------------------------
// ReadOlympusDate :  
//-------------------------------------------------------------------------
// Read Date in Olympus LS-P2 dictaphone wav files 
int16_t ReadOlympusDate(char* pText, uint16_t* ClapTime)
{
	// ClapTime indexes :
	#define Year		0 
	#define Month		1
	#define Day			2
	#define Hour		3
	#define Minute		4
	#define Second		5
	#define Millisecond	6

	ClapTime[Year] = Car2Num(pText);				// Add remaining years (no overflow)
	pText++; pText++;
	ClapTime[Month] = Car2Num(pText);				// Adjust months with overflow
	pText++; pText++;
	ClapTime[Day] = Car2Num(pText);					// Adjust days with overflow
	pText++; pText++;
	ClapTime[Hour] = Car2Num(pText);				// Adjust hours with overflow
	pText++; pText++;
	ClapTime[Minute] = Car2Num(pText);				// Adjust minutes with overflow
	pText++; pText++;
	ClapTime[Second] = Car2Num(pText);				// Adjust seconds with overflow
	pText++; pText++;
	ClapTime[Millisecond] = Car2Num(pText);		// Adjust milliseconds with overflow

	return (ValidFileTime(ClapTime));
}

//-------------------------------------------------------------------------
// Car2Num :  convert 2 char to number
//-------------------------------------------------------------------------
// return -1 if error
int16_t Car2Num(char* TNum)
{
	if (TNum[0] < '0') return(-1); // '0' = 48
	if (TNum[0] > '9') return(-1);
	if (TNum[1] < '0') return(-1);
	if (TNum[1] > '9') return(-1);
	return (TNum[0] * 10 + TNum[1] - 528);
}

//-------------------------------------------------------------------------
// Car3Num :  convert 3 char to number
//-------------------------------------------------------------------------
// return -1 if error
int16_t Car3Num(char* TNum)
{
	if (TNum[0] < '0') return(-1); // '0' = 48
	if (TNum[0] > '9') return(-1);
	if (TNum[1] < '0') return(-1);
	if (TNum[1] > '9') return(-1);
	if (TNum[2] < '0') return(-1);
	if (TNum[2] > '9') return(-1);
	return (TNum[0] * 100 + TNum[1] * 10 + TNum[2] - 5328);
}

//-------------------------------------------------------------------------
// ValidFileTime 
//-------------------------------------------------------------------------

int16_t ValidFileTime(uint16_t* FileTime)
{
	if ((FileTime[0] < 2015) || (FileTime[0] > 2035)) return (-1); // Year
	if (FileTime[1] > 12) return (-1); // Month
	if (FileTime[2] > 31) return (-1); // Day
	if (FileTime[3] > 23) return (-1); // Hour
	if (FileTime[4] > 59) return (-1); // mn
	if (FileTime[5] > 59) return (-1); // s
	return(1);
}

//-------------------------------------------------------------------------
// ClearDaiBinInfos 
//-------------------------------------------------------------------------
// Clears DaiBlocksInfo structures for Debug
void ClearDaiBinInfos(void)
{
	memset((char*)DaiBlocksInfo,0,3*sizeof(DaiBlock_Struct));
}