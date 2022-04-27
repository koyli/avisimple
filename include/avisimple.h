enum {
    YUYV = 0,
    NV12 = 1
};



namespace AviFileWriter {

    void init_avi(const char filename[], int w, int h, int fps = 30, int format = YUYV);
    void writeHeader();

    void writeHeaderHex();
    void addFrame(const void*p, int size);
    
}
