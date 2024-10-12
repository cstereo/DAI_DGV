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

#ifndef WAVIO_H
#define WAVIO_H
#include <stdio.h>
#include <stdint.h> // for int16_t and int32_t

#define NChunkMax 3

struct WavHeader_Struct // 44 bytes
{
	char		ChunkId[4];         // 'ChunkId, “RIFF”, Marks the file as a riff file. Characters are each 1 byte long.
	uint32_t	ChunkSize;			// 'ChunkSize, File size (integer); Size of the overall file – 8 bytes, in bytes (32-bit integer). Typically, you’d fill this in after creation.
	char		Format[4];			// 'Format, "WAVE" ; File Type Leader. For our purposes, it always equals “WAVE”
	char		Subchunk1Id[4];		// 'Subchunk1ID, “fmt “ ; Format chunk marker. Includes trailing null
	uint32_t	Subchunk1Size;		// 'Subchunk1IDSize,16 ; Length of format data as listed above, 16 for PCM   
	uint16_t	AudioFormat;		// 'AudioFormat, 1 ; Type of format (1 is PCM) – other are compressed formats
	uint16_t	NumChannels;        // 'NumChannels ; Number of Channels – 2 bytes integer
	uint32_t	SampleRate;         // 'SampleRate, 44100 ; Sample Rate – 4 bytes integer. Common values are 44100 (CD), 48000 (DAT). Sample Rate = Number of Samples per second, or Hertz.
	uint32_t	ByteRate;			// 'ByteRate, 176400 ; Sample Rate * BitsPerSample * Channels) / 8.
	uint16_t	BlockAlign;			// 'BlockAlign, 4 ; (BitsPerSample * Channels)/8, 1–8 bit mono, 2–8 bit stereo, 3-16 bit mono, 4–16 bit stereo
	uint16_t	BitsPerSample;		// 'BitsPerSample, 16  Bits per sample
};

struct WavSubchunk_Struct
{
	char		SubchunkId[4];		// 'SubChunk2Id "data" ;  chunk header. Marks the beginning of the data section.
	uint32_t	SubchunkSize;		// 'Subchunk2Size, File size(data) ; Size of the data section
};

struct Wav_Struct
{
	WavHeader_Struct Head;
	char* CurrentSubChunksData[NChunkMax];
	int16_t NSub; // Number of Subchunks, 1 if only "data " is available
	uint32_t DataPos;
	int32_t BytesPerAcquisition;
	int16_t SampleLen; // ByteLen
	int32_t SamplesPerChannel;
	// uint16 NChannels;
	uint16_t Time[7];
};

#endif