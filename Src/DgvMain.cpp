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

#include "stdio.h"
#ifdef _WIN32 // __unix__
	#include <windows.h>
#endif

#include <string.h>
#include <ctype.h> // used for toupper
#include <stdbool.h>

#include "FilesIO.h"
#include "DgvMain.h"
#include "WavOut.h"
#include "WavIn.h"


//-------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------
int8_t   Glob_PosInFile;
int8_t   Glob_PosInBlock;
int8_t   Glob_BlockI;
uint16_t  Glob_DaiHw;

//---------------
// Processed position in program / table information 
uint16_t Glob_InterK7ReadDelay; // Delay in CpuCycles between return and call of Read Bit function, including Enter & Exit delays

//-------------------------------------------------------------------------
// Local variables
//-------------------------------------------------------------------------


//-------------------------------------------------------------------------
// Local functions
//-------------------------------------------------------------------------
void PrintHelp(int16_t NErr);
int16_t DgvCommand(const char* FileSearchIn, const char* FileOut, const char* Options);


bool IsSameStringEnd(const char* StringIn, const char* StringEnd);
void DgvOutVersions(char* FileIn);
void SetVersion(int16_t Ver);

int16_t StrCmpUp(const char* S1,const char* S2);
void ChangeFileExt(const char* NewExt, const char* FileNameIn, char* FileNameOut);
void InsertStringBefExt(const char* InsertS, const char* FileNameIn, char* FileNameOut);


//=========================================================================
// PROGRAM
//=========================================================================
// Main is probably he only function which is OS dependant and requires string manipulation
int main(int argc, char** argv)
{
	int16_t  NErr = 0;
	uint16_t UpdatedOptionBits = 0;
	uint8_t Argi = 0;
	uint8_t NArgNames = 0;
	uint16_t DaiHwI ;
	uint16_t DaiHwI_BitI; 
	DaiBlocksInfo[0].Block = NULL; // For debug ?
	DaiBlocksInfo[1].Block = NULL;
	DaiBlocksInfo[2].Block = NULL;
	SetWavOutParameters(DaiHW_Default); // Default values 	

	if ((argc == 2)&&(strcmp(argv[1], "?") == 0))
	{
			NErr = -HelpRequestErr;
			goto ExitMain;
	}
	NArgNames = argc - 1;
	// Find options in arguments, if found, UpdatedOptionBits!=0 and parameter is in argv[Argi]
	// 
	while (((Argi+1) < argc) && (UpdatedOptionBits == 0)) // only one argument has parameters 
	{
		Argi++;
		UpdatedOptionBits = LoadProgOptionsArgument(argv[Argi]); // WavOut_NameOptions is cleaned
	}
	if (UpdatedOptionBits != 0)
	{
		NArgNames--;
	}
	// UpdatedOptionBits = UpdatedOptionBits & (~(uint16_t)OptionBit_OptionArgument); // Useless 

	if (NArgNames==0)
	{
		// Transform all .wav in .dai, and .dai in .wav
		UpdatedOptionBits = LoadProgOptionsArgument(argv[Argi]);
		Update_WavOut_NameOptions(WavOut_NameOptions);
		NErr = DgvCommand("*.wav", "*.dai", WavOut_NameOptions);
		// Transform all .dai in _Dgv.wav
		DaiHwI_BitI = 1;
		for (DaiHwI = 0; DaiHwI < 16; DaiHwI++)
		{
			if ((DaiHwI_BitI & DaiHwI_BitMask) != 0)
			{
				SetWavOutParameters(DaiHwI);
				UpdatedOptionBits = LoadProgOptionsArgument(argv[Argi]);
				Update_WavOut_NameOptions(WavOut_NameOptions);
				NErr = DgvCommand("*.dai", "*.wav", WavOut_NameOptions);
			}
			DaiHwI_BitI = DaiHwI_BitI  << 1 ;
		}
	}
	else if (NArgNames == 1)
		{
			UpdatedOptionBits = LoadProgOptionsArgument(argv[Argi]);
			Update_WavOut_NameOptions(WavOut_NameOptions);
			if (IsSameStringEnd(argv[1], ".dai"))
			{
				NErr = DgvCommand(argv[1], "*.wav", WavOut_NameOptions);
			}
			else if(IsSameStringEnd(argv[1], ".wav"))
			{
				NErr = DgvCommand(argv[1], "*.dai", WavOut_NameOptions);
			}
			goto ExitMain;
		}
	else if (NArgNames==2)
	{
		UpdatedOptionBits = LoadProgOptionsArgument(argv[Argi]); 
		Update_WavOut_NameOptions(WavOut_NameOptions);
		NErr = DgvCommand(argv[1], argv[2], WavOut_NameOptions);
		goto ExitMain;
	}
	else 
	{
		NErr = -FileParamErr;
		goto ExitMain;
	}

ExitMain:
	PrintHelp(NErr);
	return(NErr);
}

#if(1)
//-------------------------------------------------------------------------
// DgvCommand 
//-------------------------------------------------------------------------
// Process file according to valid inputs of a 2 parameters commnad line (with an potentially additional Option parameter)
// See help below for parameters structure (ex: Option ='--AI2')
int16_t DgvCommand (const char * FileSearchIn, const char* FileOut, const char* Options)
{
	int16_t  NErr = 0;
	char WavFileName[MaxLenString+1]; 	
	char DaiFileName[MaxLenString+1];

	HANDLE hFind;
	WIN32_FIND_DATAA* FindData = NULL ;
	if (strlen(FileSearchIn) < 4) // Invalin File in or option
	{
		NErr = -InvalidCmdInputErr;
		goto DgvComErr;
	}
	// 2 arguments and possibly options
	if ( (!IsSameStringEnd(FileSearchIn, ".wav")) &&
 (!IsSameStringEnd(FileSearchIn, ".dai")) ) // Invalid extension for input files
	{
		NErr = -InvalidCmdInputErr ;
		goto DgvComErr;
	}

	FindData = (WIN32_FIND_DATAA*)malloc(sizeof(WIN32_FIND_DATAA));
	if (FindData == NULL) { return(-MemAllocErr); }
	hFind = FindFirstFileA(FileSearchIn, FindData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		// For each valid input files with name FindData->cFileName
		do
		{
			NErr = 0;
			if (NotDgvFile(FindData->cFileName)) // Do not process any Dgv file
			{
				if (IsSameStringEnd(FindData->cFileName, ".wav")) // wav to ...
				{
					if (IsSameStringEnd(FileOut, ".dai")) // wav to dai
					{
						strcpy(DaiFileName, FileOut);
						if (IsSameStringEnd(DaiFileName, "*.dai")) // Input file name will be used (without extension)
						{
							ChangeFileExt(".dai", FindData->cFileName, DaiFileName);
						}
						NErr = DgvWavIn(FindData->cFileName,1); // Read the program in memory  
						if (NErr != 0) { NErr = DgvWavIn(FindData->cFileName, 0); } // Try with the alternative parity
						if (NErr >= 0) // Write .dai file
						{
							InsertStringBefExt("_Dgv", DaiFileName, DaiFileName);
							NErr = WriteDaiFile(DaiFileName);
						}
					}
					else
					if (IsSameStringEnd(FileOut, ".wav")) // wav to Wav
					{			
						strcpy(WavFileName, FileOut);
						if (IsSameStringEnd(WavFileName, "*.wav")) // Input file name will be used (without extension)
						{
							ChangeFileExt(".wav", FindData->cFileName, WavFileName);
						}
						NErr = DgvWavIn(FindData->cFileName,1); // Read the program in memory  
						if (NErr != 0) { NErr = DgvWavIn(FindData->cFileName, 0); } // Try with the alternative parity
						if (NErr >= 0) // Write .wav file
						{
							#if(InsertWavOutOptions)
								InsertStringBefExt(DaiHW_Profile[Glob_DaiHw].ProfileName, WavFileName, WavFileName); // Insert Type of HW
								InsertStringBefExt(Options, WavFileName, WavFileName); // Insert User Options
							#endif
							NErr = DgvWavOut(WavFileName);
						}
					}
					if (NErr < 0)
					{
						printf("Error %000d while processing file: %s", NErr, FindData->cFileName);
					}
				}
				else
				if ((IsSameStringEnd(FindData->cFileName, ".dai"))&&(IsSameStringEnd(FileOut, ".wav"))) // dai to Wav
				{
					strcpy(DaiFileName, FindData->cFileName);
					if (IsSameStringEnd(DaiFileName, "_Dgv.dai"))
					{
						DaiFileName[strlen(DaiFileName) - 8] = '\0';
						strcat(DaiFileName, ".dai");
					}
					strcpy(WavFileName, FileOut);
					if (IsSameStringEnd(WavFileName,"*.wav")) // Input file name will be used (without extension)
					{
						ChangeFileExt(".wav", DaiFileName, WavFileName);
					}
					NErr = ReadDaiFile(FindData->cFileName); // Read the program in memory  
					if (NErr >= 0) // Write .wav file
					{
						#if(InsertWavOutOptions)
							InsertStringBefExt(DaiHW_Profile[Glob_DaiHw].ProfileName, WavFileName, WavFileName); // Insert Type of HW
							InsertStringBefExt(Options, WavFileName, WavFileName); // Insert User Options
						#endif
						NErr = DgvWavOut(WavFileName);
					}
					if (NErr < 0)
					{
						printf("Error %000d while processing file: %s", NErr, FindData->cFileName);
					}
				}
				else
				{
					NErr = -InvalidCmdInputErr; // dai to dai -> error for the time being
					break;
				}
			}

		} while (FindNextFileA(hFind, FindData) != 0);
	}

DgvComErr:
	if (FindData != NULL) free(FindData);
	return(NErr);
}


//-------------------------------------------------------------------------
// PrintHelp 
//-------------------------------------------------------------------------
// Print help when NErr<0 (asked by user or when error in command parameters)
void PrintHelp(int16_t NErr) 
{
	if ((NErr < 0)&&(NErr>=-InvalidCmdOption))
	{
		printf("\n");
		printf("===================================================================================================\n");
		printf("Dgv generates dai files (from wav files) or optimized wav files (from dai or wav files) \n");
		printf("'Dgv InputName.xxx OutputName.yyy --Options', Src and Dest can be equal to '*', xxx/yyy = dai or wav\n");
		printf("'--Options', optional argument for optimized wav output files formed with '--' followed by several parameters:\n");
		printf("    - Vx: x=optimized wav (except V0) profile version corresponding to an hardware evironment (Dai + audio player)\n");
		printf("         V0=DaiK7_24KHz, V1=DaiV4_48KHz, for V4 with V0 being similar to a Dai ouput\n");
		printf("         V2=DAIV7A_48KHz, V3=DAIV7B_96KHz, V4=DAIV7C_384KHz for Dai V7\n");
		printf("         V5=DAIV7T_192KHz, V6=DAIV7U_384KHz, for modified Dai V7 (LM324 replace by TLC274)\n");
		printf("         V7=MameA_96KHz for Mame emulator\n");
		printf("         Speed gain vs V0: 1=3.7x, 2=3.7x, 3=4.3x, 4=4.8x, 5=6.3x, 6=7.7x, 7=9.4x, \n");
		printf("    - B=1 Bytes, W=2 Bytes, M=Mono, S=Stereo, N=Non inverted wav signal, I=Inverted wav signal output (useless for Mame)\n");
		printf("    - Fx= with x the sampling frequency in Hz (5-7 chars, example: x=96000 for Mame)\n");
		printf("'Dgv ?' For help. Dgv v0.1.0\n\n");
		printf("- Ex. in Windows terminal: 'Dgv Pacman.wav *.dai', 'Dgv Pacman.dai Pac.wav', 'Dgv *.wav *.wav --V9MBN'\n");
		printf("- Ex. in Windows terminal: 'Dgv *.wav *.wav --V3SWIF192000'\n");
		printf("- Ex. double click on Dgv.exe in windows will process all files in directory (bin and wav)\n");
		printf("Dgv v0.2.0, 12/10/2024\n");
		printf("===================================================================================================\n");
	}
}


//-------------------------------------------------------------------------
// NotDgvFile
//-------------------------------------------------------------------------
// Output false if FileName contains one of Dgv extensions (see DgvNameExtension) or is void ("")
bool NotDgvFile(char* FileName)
{
	uint8_t i;
	uint16_t FileNameLen;
	uint16_t ExtLen;
	FileNameLen = (uint16_t)strlen(FileName);
	if (FileNameLen == 0) return (false);

	for (i = 0; i < DaiHW_Count; i++)
	{
		ExtLen = (uint16_t)strlen(DaiHW_Profile[i].ProfileName);
		if ((ExtLen + 4) < FileNameLen)
		{
			if ((strstr(FileName, DaiHW_Profile[i].ProfileName) != NULL) && (StrCmpUp(FileName + FileNameLen - 4, ".wav") == 0))
				return (false);
		}
	}
	return (true);
}


//-------------------------------------------------------------------------
// DgvOutVersions 
//-------------------------------------------------------------------------
// Not used normally
// For testing / debugging different output parameters using an already downloaded file
// Input : file with full name "FileIn", I.e. with extension .dai or .wav
//
void DgvOutVersions(char * FileIn)
{
	int16_t VersionI;
	int16_t NErr;
	char FileOut[MaxLenString+1];
	char VersionString [6] ; // 5, +2 for end of string & _
	#define VerFileMax 1 // 1= no version. Used to create different versions of a file (for example with different margins)
	for (VersionI = 1; VersionI < VerFileMax; VersionI++) 
	{
			ChangeFileExt((char*)".wav", FileIn, FileOut);
			sprintf(VersionString, "_%000d", VersionI);
			InsertStringBefExt(VersionString, FileIn, FileOut);
			SetVersion(VersionI);
			NErr = DgvWavOut(FileOut);
	}
}


//-------------------------------------------------------------------------
// StrCmpUp 
//-------------------------------------------------------------------------
// String comparaison on strings 
// Same as strcmp but on upper case of string inputs
// (return 0 if equal)
int16_t StrCmpUp(const char* S1, const char* S2)
{
	uint16_t i;
	uint16_t S1L;
	uint16_t S2L;

	char S1Up[MaxLenString+1];
	char S2Up[MaxLenString+1];
	S1L = (uint16_t) strlen(S1);
	if (S1L >= MaxLenString+1) { return (-1); }
	S2L = (uint16_t) strlen(S2);
	if (S2L >= MaxLenString+1) { return (-1); }

	// Comparaison is done with upper case
	for (i = 0; i < S1L; i++)
	{
		S1Up[i] = toupper(S1[i]);
	}
	S1Up[i]='\0';
	for (i = 0; i < S2L; i++)
	{
		S2Up[i] = toupper(S2[i]);
	}
	S2Up[i] = '\0';
	return ((int16_t)strcmp(S1Up, S2Up));
}


//-------------------------------------------------------------------------
// SetVersion 
//-------------------------------------------------------------------------
// Adjust parameters according to version
void SetVersion(int16_t Ver)
{
	uint16_t i;
	int16_t del = 0;

	for (i = 0; i < 7; i++)
	{
		OutBkInterCallsDelaysMargin[i] = 0; // 40, 7 values
	}
	PeriodsOffset_Delay[0] = ((Ver & 1) != 0 ? del : 0);
	PeriodsOffset_Delay[1] = ((Ver & 2) != 0 ? del : 0);
	PeriodsOffset_Delay[2] = ((Ver & 4) != 0 ? del : 0);
	PeriodsOffset_Delay[3] = ((Ver & 8) != 0 ? del : 0);
}


//-------------------------------------------------------------------------
// InsertStringBefExt 
//-------------------------------------------------------------------------
// Insert a string before the '.' of an extension file
// Input :FileNameIn and FileNameOut strings can be identical
// Output : FileNameOut = InsertS inserted in FileNameIn ; "" if no extension of 3 characters
// Example : InsertStringBefExt("1", "Pac.dai", File) -> File = "Pac1.dai"
void InsertStringBefExt(const char * InsertS, const char* FileNameIn, char* FileNameOut)
{
	uint16_t PosDot;
	char ExtS[5];
	PosDot = (uint16_t) strlen(FileNameIn);
	if ((PosDot <=4) || (FileNameIn[PosDot-4]!='.'))
	{
		FileNameOut[0] = '\0'; 
		return;
	}
	PosDot -= 4;
	strcpy(FileNameOut, FileNameIn);
	strcpy(ExtS, FileNameIn+PosDot); // Copy last 4 characters (extension)
	strcpy(FileNameOut+ PosDot, InsertS); // Insert string 
	strcpy(FileNameOut+ PosDot+strlen(InsertS), ExtS); // Add again extension
}


//-------------------------------------------------------------------------
// ChangeFileExt 
//-------------------------------------------------------------------------
// Change File extension
// Input : NewExt, string with '.', and at least one letter
void ChangeFileExt(const char* NewExt, const char* FileNameIn, char* FileNameOut)
{
	uint16_t PosDot;
	PosDot = (uint16_t) strlen(FileNameIn);
	if ((PosDot <= 4) || (FileNameIn[PosDot - 4] != '.')|| strlen(NewExt)<=1)
	{
		FileNameOut[0] = '\0';
		return;
	}
	PosDot -= 4;
	strcpy(FileNameOut, FileNameIn);
	FileNameOut[PosDot] = '\0'; // Forget last 4 characters (extension)
	strcat(FileNameOut, NewExt);
}


//-------------------------------------------------------------------------
// IsSameStringEnd 
//-------------------------------------------------------------------------
// Check that StringEnd corresponds to the end of a string StringIn
// Used to check extensions, case senstitive
// Input : StringIn & StringEnd two string
// Output : true if strictly equal (case sensitive), false if not or LenE length is 0
// 
bool IsSameStringEnd(const char* StringIn, const char* StringEnd)
{
	uint16_t LenI;
	uint16_t LenE;
	LenI = (uint16_t)strlen(StringIn);
	LenE = (uint16_t)strlen(StringEnd);
	if ((LenE == 0) || (LenE > LenI) || (StrCmpUp(StringIn + (LenI - LenE), StringEnd) != 0))
	{
		return (false);
	}
	return(true);
}


//-------------------------------------------------------------------------
// SwapBytes
//-------------------------------------------------------------------------
// Swaps bytes in a 2 bytes word
uint16_t SwapBytes(uint16_t Word)
{
	return (((Word & 0xFF) << 8) + (Word >> 8));
}


//-------------------------------------------------------------------------
// DaiByteCheckSum 
//-------------------------------------------------------------------------
// Calculate Dai checksum of a new byte in a array
// Input : new byte and previous checksum
// Output : new Dai checksum
// Dai check sum definition : 
// - a 0x56 seed 
// - xor with previous byte, followed by a 8 bit rotation left 
uint8_t DaiByteCheckSum(uint8_t Data, uint8_t ChkSum)
{
	uint8_t bitxor;
	bitxor = Data ^ ChkSum;
	return ((bitxor << 1) + ((bitxor & 0x80) != 0));
}


//-------------------------------------------------------------------------
// DaiByteCheckSum
//-------------------------------------------------------------------------
// return Dai checksum of a word of 2 Bytes 
uint8_t DaiWordCheckSum(uint16_t Word)
{
	uint8_t ChkSum = 0x56;
	ChkSum = DaiByteCheckSum(Word >> 8, ChkSum); // High byte
	ChkSum = DaiByteCheckSum(Word & 0xFF, ChkSum); // Low byte
	return (ChkSum);
}

#endif