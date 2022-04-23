typedef unsigned char BYTE;
typedef short WORD;
typedef int DWORD;
typedef DWORD LONG;

enum {
    YUYV = 0,
    NV12 = 1
};


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

    void init_avi(int w, int h, int fps = 30, int format = YUYV);
    void writeHeader();

    void writeHeaderHex();
    void addFrame(const void*p, int size);
    
}
