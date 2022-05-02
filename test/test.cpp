#include "avisimple.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>




using namespace AviFileWriter;
/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 *
 * To build:
 *   g++ -std=c++11 -o v4l2-capture v4l2-capture.cpp
 */

#include <iostream>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

using namespace std;

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define TEST_COUNT 3
const char* testfiles[TEST_COUNT] = { "test/1.jpg", "test/2.jpg", "test/3.jpg" };

char* images[TEST_COUNT];
int sizes[TEST_COUNT];


static const char      *dev_name;
static int              fd = -1;
static int              frame_count = 200;
static int              frame_number = 0;

static string output_name;
static int req_width = 0;
static int req_height = 0;
static int req_rate_numerator   = 30;
static int req_rate_denominator = 1;


static void process_image(const void *p, int size)
{
    addFrame(p, size);
}

static int read_frame(void)
{
    static unsigned int i = 0;

    process_image(images[i % TEST_COUNT], sizes[i % TEST_COUNT]);

    ++i;
    return 0;
}

static void mainloop(void)
{
    unsigned int count;

    count = frame_count;

    while (count-- > 0) {

            if (read_frame())
                break;
    }
}

static void stop_capturing(void)
{
}

static void start_capturing(void)
{
}

int main(int argc, char **argv)
{
    req_width = 720;
    req_height = 350;





    for (int i= 0; i < TEST_COUNT; ++i) {
        int fd;
        struct stat st;
        fd = open(testfiles[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "missing input file");
            exit(-1);
        }
        fstat(fd, &st);
        sizes[i] = st.st_size;
        images[i] = (char*)calloc(st.st_size, 1);
        int r = read(fd, images[i], st.st_size);
        if (r < st.st_size) {
            fprintf(stderr, "short read of input file");
            exit(-1);
        }
             
    }



    std::string pattern = "/tmp/video-capXXXXXX.avi";
    char pattern_copy[pattern.length() + 1];
    memcpy(pattern_copy, pattern.c_str(), pattern.length() + 1);
    int fd_temp;
    if ((fd_temp = mkstemps(pattern_copy, 4)) == -1) {
        close(fd_temp);
    }

    init_avi(pattern_copy, req_width, req_height, 30, NV12);

    start_capturing();
    mainloop();
    stop_capturing();


    fprintf(stderr, "\n");
    writeHeader();
    return 0;
}


// int main(int argc, char** argv) {

//     unsigned int dataSize = 0;

//     init();
//     if (argc > 1 && strcmp(argv[1], "-hex")== 0)
//         writeHeaderHex();
//     else
//         writeHeader();


//     int fd = open("/dev/video0", O_RDWR);

//     return 0;
// }
