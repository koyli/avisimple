#include "avisimple.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <ext/stdio_filebuf.h>
#include <sys/types.h>
#include <unistd.h>


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
    
    int fd;
    int file_length;
    
    void init_avi(int w, int h, int fps, int format) {

        file_length = sizeof(block); 

        std::string pattern = "video-capXXXXXX.avi";
        char pattern_copy[pattern.length() + 1];
        memcpy(pattern_copy, pattern.c_str(), pattern.length() + 1);
        if ((fd = mkstemps(pattern_copy, 4)) == -1) {
            std::cerr << "Error opening temp file" << std::endl;
            exit(1);
        }
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

    };
    
    char hex(unsigned char c)
    {
        if (c >= 10)
            return 'a' + (c - 10);
        else return '0' + c;
    }


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
        int pad = (size & 3) ? (size % 4) + 4 : size;
        write(fd, padding, pad - size);
        file_length += pad  + 8;
        block.movi.listSize = file_length - sizeof(block);
        block.header.fileSize = file_length;
        
        block.avih.dwTotalFrames++;
        block.strh.dwLength++;
        writeHeader();
    }
    
        
}

