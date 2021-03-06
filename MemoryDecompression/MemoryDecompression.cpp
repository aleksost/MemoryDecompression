#include "pch.h"
#include <iostream>
#include <Windows.h>
#include <fstream>
#include <string> 
#include <strsafe.h>
#include <chrono>


#define STATUS_SUCCESS                 ((NTSTATUS)0x00000000UL)
#define STATUS_BUFFER_ALL_ZEROS        ((NTSTATUS)0x00000117UL)
#define STATUS_INVALID_PARAMETER       ((NTSTATUS)0xC000000DUL)
#define STATUS_UNSUPPORTED_COMPRESSION ((NTSTATUS)0xC000025FUL)
#define STATUS_NOT_SUPPORTED_ON_SBS    ((NTSTATUS)0xC0000300UL)
#define STATUS_BUFFER_TOO_SMALL        ((NTSTATUS)0xC0000023UL)
#define STATUS_BAD_COMPRESSION_BUFFER  ((NTSTATUS)0xC0000242UL)

HMODULE ntdll = GetModuleHandle(TEXT("ntdll.dll"));

typedef NTSTATUS(__stdcall *_RtlDecompressBuffer)(
	USHORT CompressionFormat,
	PUCHAR UncompressedBuffer,
	ULONG UncompressedBufferSize,
	PUCHAR CompressedBuffer,
	ULONG CompressedBufferSize,
	PULONG FinalUncompressedSize
	);

// roundUp is used for finding the next sensible offset for a new chunk of data to decompress
int roundUp(int numToRound, int multiple)
{
	int remainder = abs(numToRound) % multiple;
	return numToRound + multiple - remainder;
}

// ----------------------------------------------------------------------------------------------------

BOOL decompress_buffer(UCHAR *buffer, const int bufferLen, const int uncompBufferLen, ULONG *uncompBufferSize, UINT *nextOffset, UCHAR *uncompBuffer, UINT *compress_counter)
{
	BOOL success = false;
	_RtlDecompressBuffer RtlDecompressBuffer = (_RtlDecompressBuffer)GetProcAddress(ntdll, "RtlDecompressBuffer");

	//UCHAR *uncompBuffer = new UCHAR[uncompBufferLen];
	NTSTATUS result = RtlDecompressBuffer(
		COMPRESSION_FORMAT_XPRESS | COMPRESSION_ENGINE_STANDARD, // CompressionFormat
		uncompBuffer,										     // UncompressedBuffer
		uncompBufferLen,										 // UncompressedBufferSize
		buffer,												   	 // CompressedBuffer
		bufferLen,											     // CompressedBufferSize
		uncompBufferSize										 // FinalUncompressedSize
	);

	if (result == STATUS_SUCCESS) {
		*compress_counter += 1;
		if (*uncompBufferSize == (ULONG)4096) {
			//std::cout << "Compressed Size: " << bufferLen << std::endl;
			//std::cout << "Counter: " << *compress_counter << std::endl;
			//std::cout << "Uncompressed Size: " << *uncompBufferSize << std::endl;

			*nextOffset = roundUp(bufferLen, 16);
			//std::cout << "Distance to next compressed data: " << *nextOffset << std::endl;
			success = true;
		}
	}
	return success;
}

// ----------------------------------------------------------------------------------------------------

BOOL decompress_file(const WCHAR *input_filename, HANDLE hFileInput, UCHAR &zero_buffer, HANDLE hFileOutput, UINT *compressed_total, UINT *decompressed_pages)
{
	DWORD fileSizeHi = 0;
	DWORD fileSize = GetFileSize(hFileInput, &fileSizeHi);
	BYTE *fileBuffer = new BYTE[0x80000];

	DWORD bytesRead;
	//BOOL readSuccess = ReadFile(hFileInput, fileBuffer, fileSize, &bytesRead, NULL);
	//if (!readSuccess) return 0;

	while (ReadFile(hFileInput, fileBuffer, 0x80000, &bytesRead, NULL)) {
		if (bytesRead == 0) break;
		UINT nextOffset = 0;
		UINT nextOffsetTotal = 0;

	LOOP: while (nextOffsetTotal < bytesRead) {


		UCHAR *buffer = new UCHAR[4096];
		UCHAR *uncompBuffer = new UCHAR[4096];
		UINT j = 0; // Buffer needs to start at j = 0, since "i" starts at nextOffsetTotal which is only 0 at the first page
		for (int i = nextOffsetTotal; i < 4096 + nextOffsetTotal; i++) {
			buffer[j] = (UCHAR)fileBuffer[i];
			j++;
		}
		if (!buffer) {
			std::cout << "An error occurred while reading the bytes of the input file into the buffer." << std::endl;
			return 1;
		}

		// Checks if the current buffer is all zeroes
		int ifNull = memcmp(buffer, &zero_buffer, 4096);
		if (ifNull == 0) {
			nextOffsetTotal += 4096;
			//std::cout << "Buffer is all zeroes, jumping 4096 bytes.." << std::endl;
			delete[]uncompBuffer;
			delete[]buffer;
			goto LOOP;
		}
		UINT compression_counter = 0;
		int bufferLen = 15;
		while (bufferLen < 4096) {

			bufferLen++;

			ULONG realDecompSize = 0;

			BOOL decompressed_buffer = decompress_buffer(buffer, bufferLen, 4096, &realDecompSize, &nextOffset, uncompBuffer, &compression_counter);

			if (bufferLen > 200 && compression_counter == 0) {
				//std::cout << compression_counter << " lol " << bufferLen << std::endl;
				nextOffsetTotal += 16;
				delete[]uncompBuffer;
				delete[]buffer;
				goto LOOP;
			}

			// Write to file if a 4096 bytes page is successfully decompressed
			if (decompressed_buffer) {

				BOOL WriteFile_result = WriteFile(hFileOutput, uncompBuffer, realDecompSize, &realDecompSize, NULL);

				if (!WriteFile_result) return 0;

				*compressed_total += bufferLen;

				nextOffsetTotal += nextOffset;
				//std::cout << "Offset to next compressed data: " << nextOffsetTotal << "\n" << std::endl;
				*decompressed_pages += 1;
				break;
			}

			if (bufferLen == 4096) {
				nextOffsetTotal += 16;
				//std::cout << "Decompression failed, jumping 16 bytes to offset " << nextOffsetTotal << "\n" << std::endl;
				break;
			}

		}
		delete[]uncompBuffer;
		delete[]buffer;
	}
		  if (nextOffsetTotal >= fileSize) CloseHandle(hFileInput);

	}
	delete[]fileBuffer;
}

// ----------------------------------------------------------------------------------------------------

int main(int argc, wchar_t* argv[])
{
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
	if (argc != 3)
	{
		printf("Incorrect parameters\n\nUsage: MemoryDecompression.exe <input file or directory> <output-file>");
		return 0;
	}
	const WCHAR *input_filename = argv[1];
	const WCHAR *output_filename = argv[2];
	if (GetFileAttributesA((LPCSTR)output_filename) != INVALID_FILE_ATTRIBUTES) {
		printf("Output file already exists..");
		return 0;
	}
	// Creates handle to output file
	HANDLE hFileOutput = CreateFile((LPCSTR)output_filename, FILE_APPEND_DATA, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFileOutput == INVALID_HANDLE_VALUE) {
		printf("Failed to create output file");
	}

	UINT decompressed_pages = 0;
	UINT compressed_total = 0;
	UCHAR zero_buffer[0x1000] = { 0 };
	DWORD input_file_attribute = GetFileAttributesA((LPCSTR)input_filename);

	// If the input file is a directory
	if (input_file_attribute & FILE_ATTRIBUTE_DIRECTORY)
	{
		WIN32_FIND_DATA ffd;
		TCHAR szDir[MAX_PATH];
		HANDLE hFind = INVALID_HANDLE_VALUE;

		StringCchCopy(szDir, MAX_PATH, (STRSAFE_LPCSTR)input_filename);
		StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

		hFind = FindFirstFile(szDir, &ffd);
		FindNextFile(hFind, &ffd);

		SetCurrentDirectory((LPCSTR)input_filename);

		while (FindNextFile(hFind, &ffd) != 0) {
			// Creates handle to input file
			printf("Decompressing\t %s\n", ffd.cFileName);
			HANDLE hFileInput = CreateFile((LPCSTR)&ffd.cFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFileInput == INVALID_HANDLE_VALUE) {
				printf("Failed to open file input file\n");
			}
			BOOL decompress_file_result = decompress_file(input_filename, hFileInput, *zero_buffer, hFileOutput, &compressed_total, &decompressed_pages);

		}
	}

	// If the input file is not a directory
	else
	{
		// Creates a handle to the input file
		HANDLE hFileInput = CreateFile((LPCSTR)input_filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFileInput == INVALID_HANDLE_VALUE) {
			printf("Failed to open input file");
		}
		printf("Decompressing \t %s\n", input_filename);
		BOOL decompress_file_result = decompress_file(input_filename, hFileInput, *zero_buffer, hFileOutput, &compressed_total, &decompressed_pages);
	}
	// Print statistics
	std::cout << "\n" << "Total decompressed pages: \t" << decompressed_pages << std::endl;
	std::cout << "Total compressed data: \t\t" << compressed_total << " bytes" << std::endl;
	std::cout << "Total decompressed data: \t" << decompressed_pages * 4096 << " bytes" << std::endl;
	CloseHandle(hFileOutput);
	std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
	auto duration_ms = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
	auto duration_sec = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count();
	auto duration_min = std::chrono::duration_cast<std::chrono::minutes>(t2 - t1).count();
	auto duration_hrs = std::chrono::duration_cast<std::chrono::hours>(t2 - t1).count();

	std::cout << "\n\nDecompression completed in:\nTotal Microseconds:\t" << duration_ms;
	std::cout << "\nTotal Seconds:\t" << duration_sec;
	std::cout << "\nTotal Minutes:\t" << duration_min;
	std::cout << "\nTotal Hours:\t" << duration_hrs;
}
