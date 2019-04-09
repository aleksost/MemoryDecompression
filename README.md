# MemoryDecompression
Tool to decompress data from Windows 10 page files and memory dumps, that has been compressed by the Windows 10 memory manager. MemoryDecompression.exe can be run on Windows 8 and 10, as earlier versions of Windows does not support the RtlDecompressBuffer function with the Xpress algorithm. 

## Usage
``` MemoryDecompression.exe <input file or directory> <output-file> ```\
When the input is a directory, the tool decompresses all the containing files. 

### Example on a memory dump:
The tool can be run on directly on a memory dump, but this can take a really long time, and has not been tested much at this point. I recommend using a tool like Volatility to dump the Virtual Address Descriptors from the MemCompression process (vaddump: https://github.com/volatilityfoundation/volatility/wiki/Command-Reference#vaddump). The vaddump plugin requires a pid, an offset or a name to properly dump the MemCompression process, and an output directory. After dumping it to the case folder, run MemoryCompression with the vaddump directory as input. The decompressed data is always written to a single output file.

```ps
PS > .\MemoryDecompression.exe "C:\Users\Analyst\CaseXYZ\vaddump" "C:\Users\Analyst\CaseXYZ\vaddump-decompressed.bin"
...
Decompressing    MemCompression.7e30e5c0.0x000000000b670000-0x000000000b68ffff.dmp
Decompressing    MemCompression.7e30e5c0.0x000000000b690000-0x000000000b6affff.dmp
Decompressing    MemCompression.7e30e5c0.0x000000000b6b0000-0x000000000b6cffff.dmp
Decompressing    MemCompression.7e30e5c0.0x000000000b6d0000-0x000000000b6effff.dmp
Decompressing    MemCompression.7e30e5c0.0x000000000b6f0000-0x000000000b70ffff.dmp

Total decompressed pages:       126619
Total compressed data:          164234863 bytes
Total decompressed data:        518631424 bytes


Decompression completed in:
Total Microseconds:     342921837
Total Seconds:  342
Total Minutes:  5
Total Hours:    0
```

### Example on a page file:
This example is performed on a real page file. The file was ~6GB, and found ~1GB of compressed data that decompressed to ~3GB.
```ps
PS > .\MemoryDecompression.exe .\pagefile.sys .\pagefile-decompressed.bin
Decompressing    .\pagefile.sys

Total decompressed pages:       803827
Total compressed data:          1111505498 bytes
Total decompressed data:        3292475392 bytes


Decompression completed in:
Microseconds:   16731799969
Seconds:        16731
Minutes:        278
Hours:  4
```

### Example on a VADDUMP-segment:
```ps
PS > .\MemoryDecompression.exe "VADDUMP-segment-compressed.bin" "VADDUMP-segment-decompressed.bin"
Decompressing    VADDUMP-segment-compressed.bin

Total decompressed pages:       130
Total compressed data:          122664 bytes
Total decompressed data:        532480 bytes


Decompression completed in:
Total Microseconds:     552337
Total Seconds:  0
Total Minutes:  0
Total Hours:    0
```
