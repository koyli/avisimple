#include "avisimple.h"
#include <cstring>

#ifdef ARDUINO
#include <SD_MMC.h>
#define DEBUG(x) Serial.println(x)
#else
#include <fstream>
#include <iostream>
#include <ext/stdio_filebuf.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#define DEBUG(x) printf(x)
#endif


typedef unsigned char BYTE;
typedef short WORD;
typedef int DWORD;
typedef DWORD LONG;
#define AVIF_HASINDEX 0x00000010

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

    int index_size = 0;   

    
#ifndef ARDUINO
    int fd;
    int fd_idx;
#else
    File fd, fd_idx;
#endif
    int file_length;
#ifdef ARDUINO    
    File init_avi(const char filename[], int w, int h, int fps, int format) {
#else
    int init_avi(const char filename[], int w, int h, int fps, int format) {
#endif        
        file_length = sizeof(block); 

#ifndef ARDUINO
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR );
        std::string pattern = "/tmp/video-XXXXXX.idx";
        char pattern_copy[pattern.length() + 1];
        memcpy(pattern_copy, pattern.c_str(), pattern.length() + 1);
        if ((fd_idx = mkstemps(pattern_copy, 4)) == -1) {
            return -1;
        }

#else
        fd = SD_MMC.open(filename, FILE_WRITE);
        if (SD_MMC.exists("/vid.idx"))
            SD_MMC.remove("/vid.idx");
            
        fd_idx = SD_MMC.open("/vid.idx", FILE_WRITE);

        if (!fd || !fd_idx) {
            DEBUG("Failed to open file for writing");
            return fd;
        }
#endif
        
        block.header.RIFF = {'R', 'I', 'F', 'F' };
        block.header.fileType = {'A', 'V', 'I', ' ' };
        block.header.fileSize = sizeof(block) - 8 + block.movi.listSize;

        block.hdrl.LIST = {'L', 'I', 'S', 'T' };
        block.hdrl.listType = {'h', 'd', 'r', 'l' };
        block.hdrl.listSize = (char*) &block.movi - (char*) &block.avih + sizeof(block.hdrl.listSize);

        block.avih.fcc = {'a', 'v', 'i', 'h' };
        block.avih.cb = sizeof(MainAVIHeader) - 8;
        block.avih.dwStreams = 1;
        block.avih.dwWidth = w;
        block.avih.dwHeight = h;
        block.avih.dwMicroSecPerFrame = 10e6 / fps;
        block.avih.dwFlags |= AVIF_HASINDEX;
        
        block.strl.LIST = {'L', 'I', 'S', 'T' };
        block.strl.listType = {'s', 't', 'r', 'l' };
        block.strl.listSize = (char*) &block.movi - (char*) &block.strhh + sizeof(block.strl.listType);

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
            block.strf.biBitCount = 8 * 3;
            //            block.strf.biCompression = {'M','J','P', 'G'};
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
        block.movi.listSize = sizeof(block.movi.listSize);

        writeHeader();
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
#define write(fd, a, b) fd.write((const uint8_t*) (a), b)
#define read(fd, a, b) fd.read((uint8_t*) (a), b)
#undef SEEK_END
#undef SEEK_SET
#define SEEK_END SeekMode::SeekEnd
#define SEEK_SET SeekMode::SeekSet
#endif
    
    void writeHeader() {
        lseek(fd, 0, SEEK_SET);


        write(fd, reinterpret_cast<char*> (&block), sizeof(block));
    }

    void addFrame(const void*p, int size) {


        const char padding[] = {0,0,0,0};
        const char header[] = {'0', '0', 'd', 'c'};
        const char idx1[] = { 'i', 'd', 'x', '1' };
        const char terminator[] = { (char)0xff,(char) 0xd9 };
        index_size += 4 * 4;

        const uint8_t* pp =         (const uint8_t*) p;
        
        int offset = file_length;
        lseek(fd_idx, 0, SEEK_SET);
        write(fd_idx, idx1, 4);
        write(fd_idx, &index_size, 4);
              
        lseek(fd, 0, SEEK_END);

        int write_size = size;

        if (size >= 2 && pp[size - 1] != terminator[1] && pp[size - 2] != terminator[0])
            {
                write_size += 2;
            }
        write(fd, header, sizeof(header));
        write(fd, (const char*) &write_size, sizeof(int));
        write(fd, (const char*) p, size);

        if (write_size != size)
            write(fd, (const char*) terminator, 2);
        
        int pad = (write_size & 3) ? ((write_size  | 3) + 1)  : write_size;
        write(fd, padding, pad - write_size);
        file_length += pad  + 8;
        block.movi.listSize = file_length - sizeof(block) + sizeof(block.movi.listSize);
        block.header.fileSize = file_length;
        
        block.avih.dwTotalFrames++;
        block.strh.dwLength++;

        if (block.avih.dwTotalFrames ^ 0x3f == 0) {
            // every 64 frame, write header again
            writeHeader();
        }

        lseek(fd_idx, 0, SEEK_END);
        write(fd_idx, header, sizeof(header));
        write(fd_idx, padding, 4);

        write(fd_idx, &offset, 4);
        write(fd_idx, &write_size, 4);

    }

    void addIndex() {
        lseek(fd, 0, SEEK_END);

#ifdef ARDUINO
        if (!fd_idx)
            DEBUG("error - index null " );

        fd_idx.close();

        fd_idx = SD_MMC.open("/vid.idx", FILE_READ);

#endif

        lseek(fd_idx, 0, SEEK_SET);
        uint8_t buff[256];
        int bytes_read = 0;

        while ((bytes_read = read(fd_idx, buff, 256)) > 0)
            write(fd, buff, bytes_read);
        
        block.header.fileSize += index_size + 8;

#ifdef ARDUINO
        fd_idx.close();
        fd_idx = SD_MMC.open("/vid.idx", FILE_WRITE);
        if (!fd_idx)
            DEBUG("error - index null " );
#else
        
#endif

    }
    void closeAvi() {
        DEBUG("Add index..");
        addIndex();
        writeHeader();
        
#ifdef ARDUINO
        fd.close();
        fd_idx.close();
#else
        close(fd);
#endif
    }
}

