enum {
    YUYV = 0,
    NV12 = 1
};



namespace AviFileWriter {

    int init_avi(const char filename[], int w, int h, int fps = 30, int format = YUYV);
    void writeHeader();
    void close();
#ifndef ARDUINO
    void writeHeaderHex();
#endif
    void addFrame(const void*p, int size);
    
}
