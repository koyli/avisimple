#include "avisimple.h"
#include <cstring>

#ifdef ARDUINO
#include <SD_MMC.h>
#else
#include <fstream>
#include <iostream>
#include <ext/stdio_filebuf.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif


typedef unsigned char BYTE;
typedef short WORD;
typedef int DWORD;
typedef DWORD LONG;


typedef struct {
    DWORD dwWidth;
    DWORD dwHeight;
} RECT;

typedef union {
    char ch[4];
    DWORD tag;
} FOURCC;
    

typedef struct {
    FOURCC RIFF;
    unsigned int fileSize;
    FOURCC fileType;
} RIFFHeader;

typedef struct {
    FOURCC LIST;
    unsigned int listSize;
    FOURCC listType;
} LISTHeader;

typedef struct {
    FOURCC fcc;
    DWORD cb;
    DWORD dwMicroSecPerFrame;

    DWORD dwMaxBytesPerSec;

    DWORD dwPaddingGranularity;
    DWORD dwFlags;
    
    DWORD dwTotalFrames;
    DWORD dwInitialFrames;

    DWORD dwStreams;

    DWORD dsSuggestedBufferSize;

    DWORD dwWidth;
    DWORD dwHeight;
    DWORD reserve[4];
} MainAVIHeader;

typedef struct {
    FOURCC fccType;
    FOURCC fccHandler;
    DWORD dwFlags;
    WORD wPriority;
    WORD wLanguage;
    DWORD dwInitialFrames;
    DWORD dwScale;
    DWORD dwRate;
    DWORD dwStart;
    DWORD dwLength;
    DWORD dwSuggestedBufferSize;
    DWORD dwQuality;
    DWORD dwSampleSize;
    RECT rcFrame;
} AVIStreamHeader;

typedef struct {
    FOURCC type;
    DWORD size;
} LISTItem;

typedef struct {
  WORD  wFormatTag;
  WORD  nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD  nBlockAlign;
  WORD  wBitsPerSample;
  WORD  cbSize;
} WAVEFORMATEX;

typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG  biWidth;
  LONG  biHeight;
  WORD  biPlanes;
  WORD  biBitCount;
  FOURCC biCompression;
  DWORD biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct _tag_size_combine {
    FOURCC name;
    DWORD size;
} TagHeader;

typedef struct {
    DWORD dwChunkId;
    DWORD dwFlags;
    DWORD dwOffset;
    DWORD dwSize;
  } _avioldindex_entry, AVIOLDINDEX_ENTRY;

typedef struct _avioldindex {
  FOURCC             fcc;
  DWORD              cb;
  _avioldindex_entry aIndex[];
} AVIOLDINDEX;

namespace AviFileWriter {

    struct SimpleAviHeaderBlock {
        RIFFHeader header;

        LISTHeader hdrl;
        MainAVIHeader avih;
        LISTHeader strl;
        FOURCC strhh;
        DWORD strhs;
        AVIStreamHeader strh;
        FOURCC strfh;
        DWORD strfs;
        BITMAPINFOHEADER strf;
        
        LISTHeader movi;
    } block;

    #define MAX_CHUNKS_SIZE 1000000

    char imageChunks[MAX_CHUNKS_SIZE];
    
#ifndef ARDUINO
    int fd;
#else
    File fd;
#endif
    int file_length;
    
    int init_avi(const char filename[], int w, int h, int fps, int format) {

        file_length = sizeof(block); 

#ifndef ARDUINO
        fd = open(filename, O_RDWR);
#else
        fd = SD_MMC.open(filename, FILE_WRITE);
#endif
        if (fd < 0) return fd;
        
        block.header.RIFF = {'R', 'I', 'F', 'F' };
        block.header.fileType = {'A', 'V', 'I', ' ' };
        block.header.fileSize = sizeof(block) - 8 + block.movi.listSize;

        block.hdrl.LIST = {'L', 'I', 'S', 'T' };
        block.hdrl.listType = {'h', 'd', 'r', 'l' };
        block.hdrl.listSize = (char*) &block.movi - (char*) &block.avih;

        block.avih.fcc = {'a', 'v', 'i', 'h' };
        block.avih.cb = sizeof(MainAVIHeader) - 8;
        block.avih.dwStreams = 1;
        block.avih.dwWidth = w;
        block.avih.dwHeight = h;
        block.avih.dwMicroSecPerFrame = 10e6 / fps;
        
        block.strl.LIST = {'L', 'I', 'S', 'T' };
        block.strl.listType = {'s', 't', 'r', 'l' };
        block.strl.listSize = (char*) &block.movi - (char*) &block.strhh;

        block.strhh = {'s', 't', 'r', 'h' };
        block.strhs = sizeof(block.strh);
        block.strh.fccType = {'v', 'i', 'd', 's' };
        block.strh.fccHandler = {'M', 'J', 'P', 'G' };
        block.strh.dwScale = 1;
        block.strh.dwRate = fps;
        
        block.strfh = {'s', 't', 'r', 'f' };
        block.strfs = sizeof(block.strf);
        block.strf.biSize = sizeof(block.strf);
        block.strf.biWidth = w;
        block.strf.biHeight = h;
        block.strf.biPlanes = 1;

        switch (format) {
        case YUYV:
            block.strf.biBitCount = 8 * 2;
            block.strf.biCompression = {'Y','U','Y', '2'};
            break;
        case NV12:
            block.strf.biBitCount = 12;
            block.strf.biCompression = {'M','J','P', 'G'};
            break;
        }            
        block.strf.biSizeImage = w * h * block.strf.biBitCount / 8 ;
        
        block.movi.LIST = {'L', 'I', 'S', 'T'};
        block.movi.listType = {'m', 'o', 'v', 'i'};
        block.movi.listSize = 0;
        return fd;
    };
    
    char hex(unsigned char c)
    {
        if (c >= 10)
            return 'a' + (c - 10);
        else return '0' + c;
    }

#ifndef ARDUINO
    void writeHeaderHex() {
        for (int i = 0 ; i < sizeof(block); ++i) {
            switch (i) {
            case 0:
                std::cout << "RIFF header:" << std::endl;
                break;
            case sizeof(RIFFHeader):
                std::cout << "HDRL:" << std::endl;
                break; 
            case sizeof(RIFFHeader) + sizeof(LISTHeader):
                std::cout << "AVIH:" << std::endl;
                break;
            case sizeof(RIFFHeader) + sizeof(LISTHeader) + sizeof(MainAVIHeader):
                std::cout << "STRL:" << std::endl;
                break;
               
            default:
                break;
            };
            unsigned char* b = reinterpret_cast<unsigned char*> (&block);
            std::cout << hex(b[i] >> 4) << hex(b[i] & 0xf) << ' ';
            if (3 == (i & 0x3))std::cout << std::endl;
        }
    }
#endif


#ifdef ARDUINO
#define lseek(fd, a, b) fd.seek(a, b)
#define write(fd, a, b) fd.write(a, b)
#endif
    
    void writeHeader() {
        lseek(fd, 0, SEEK_SET);
        write(fd, reinterpret_cast<char*> (&block), sizeof(block));
    }

    void addFrame(const void*p, int size) {
        const char padding[] = {0,0,0,0};
        const char header[] = {'0', '0', 'd', 'c'};
        lseek(fd, 0, SEEK_END);
        write(fd, header, sizeof(header));
        write(fd, (const char*) &size, sizeof(int));
        write(fd, (const char*) p, size);
        int pad = ((size & 3) ? ((size | 3) + 1)  : 0) + size;
        write(fd, padding, pad - size);
        file_length += pad  + 8;
        block.movi.listSize = file_length - sizeof(block);
        block.header.fileSize = file_length;
        
        block.avih.dwTotalFrames++;
        block.strh.dwLength++;
        //        writeHeader();
    }
    
    void close() {
#ifdef ARDUINO
        fd.close();
#else
        close(fd);
#endif
    }
}

