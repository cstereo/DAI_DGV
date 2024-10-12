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

#ifndef FILESIO_H
#define FILESIO_H
#include <stdio.h>
#include <string.h> // for memcmp
#include <stdint.h> // for int16_t and int32_t
#include "WavIO.h"

struct DaiBlock_Struct
{
	uint16_t	Len;		// Block length
	uint8_t		LenCS;      // Checksum of length
	char*		Block;		// Block of information (name, buffet, table)
	uint8_t		BlockCS;	// Checksum of block
} ;


//-------------------------------------------------------------------------
// Global variables 
//-------------------------------------------------------------------------

extern DaiBlock_Struct DaiBlocksInfo[3];
extern uint8_t Glob_ProgType;


//-------------------------------------------------------------------------
// Global functions 
//-------------------------------------------------------------------------
// Writing wav functions
int16_t CreateWavOut(FILE* WaveFile, uint32_t NSamples, uint32_t SRate, uint8_t NChannels, uint8_t Bytes_per_sample);
int16_t UdpdateWavSize(FILE* WaveFile, uint32_t NSamples, uint8_t Bytes_per_sample);

// Reading Bin functions
int16_t ReadDaiFile(char* DaiFileName);
int16_t WriteDaiFile(char* DaiFileName);
void FreeDaiBlockBuffers(void);

// Reading wav functions
int16_t ReadWavHeader(char* FileName, Wav_Struct* CurrentWav);
int16_t ReadHeaderSubChunk(FILE* WFile, Wav_Struct* CurrentWav);

// Tools
void ClearDaiBinInfos(void);
#endif