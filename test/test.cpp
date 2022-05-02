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
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

using namespace std;

#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer_t {
    void   *start;
    size_t  length;
};

static const char      *dev_name;
static int              fd = -1;
static vector<buffer_t> buffers;
static int              frame_count = 200;
static int              frame_number = 0;

static string output_name;
static int req_width = 0;
static int req_height = 0;
static int req_format = V4L2_PIX_FMT_YUYV; // yuy2
static int req_rate_numerator   = 30;
static int req_rate_denominator = 1;

static void errno_exit(const char *s)
{
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

static void process_image(const void *p, int size)
{
    addFrame(p, size);
}

static int read_frame(void)
{
        struct v4l2_buffer buf;
        unsigned int i;

        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
            switch (errno) {
                case EAGAIN:
                    return 0;

                case EIO:
                    /* Could ignore EIO, see spec. */

                    /* fall through */

                default:
                    errno_exit("VIDIOC_DQBUF");
            }
        }

        assert(buf.index < buffers.size());

        process_image(buffers[buf.index].start, buf.bytesused);

        
        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                errno_exit("VIDIOC_QBUF");

        return 1;
}

static void mainloop(void)
{
    unsigned int count;

    count = frame_count;

    while (count-- > 0) {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select(fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if (EINTR == errno)
                    continue;
                errno_exit("select");
            }

            if (0 == r) {
                fprintf(stderr, "select timeout\n");
                exit(EXIT_FAILURE);
            }

            if (read_frame())
                break;
            /* EAGAIN - continue select loop. */
        }
    }
}

static void stop_capturing(void)
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
        errno_exit("VIDIOC_STREAMOFF");

}

static void start_capturing(void)
{
        unsigned int i;
        enum v4l2_buf_type type;

        for (i = 0; i < buffers.size(); ++i) {
            struct v4l2_buffer buf;

            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                errno_exit("VIDIOC_QBUF");
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
            errno_exit("VIDIOC_STREAMON");
}

static void uninit_device(void)
{
    unsigned int i;

    for (i = 0; i < buffers.size(); ++i)
        if (-1 == munmap(buffers[i].start, buffers[i].length))
            errno_exit("munmap");

    buffers.clear();
}


static void init_mmap(void)
{
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support "
                    "memory mapping\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n",
                 dev_name);
        exit(EXIT_FAILURE);
    }

    buffers.resize(req.count);

    for (size_t idx = 0; idx < buffers.size(); ++idx) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = idx;

        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF");

        buffers[idx].length = buf.length;
        buffers[idx].start =
                mmap(nullptr /* start anywhere */,
                     buf.length,
                     PROT_READ | PROT_WRITE /* required */,
                     MAP_SHARED /* recommended */,
                     fd, buf.m.offset);

        if (MAP_FAILED == buffers[idx].start)
            errno_exit("mmap");
    }
}


static void init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    struct v4l2_streamparm streamparm;
    unsigned int min;

    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s is no V4L2 device\n",
                     dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is no video capture device\n",
                 dev_name);
        exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming i/o\n",
                 dev_name);
        exit(EXIT_FAILURE);
    }


    /* Select video input, video standard and tune here. */

#if 0
    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    } else {
            /* Errors ignored. */
    }
#endif

    CLEAR(streamparm);

    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl(fd, VIDIOC_G_PARM, &streamparm))
        errno_exit("VIDIOC_G_PARM");

    if (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
        auto &tpf = streamparm.parm.capture.timeperframe;

        // Swap numerator and denominator: rate is an inverted frame time :-)
        tpf.numerator = req_rate_denominator;
        tpf.denominator = req_rate_numerator;

        if (-1 == xioctl(fd, VIDIOC_S_PARM, &streamparm))
            errno_exit("VIDIOC_S_PARM");

        if (tpf.numerator != req_rate_denominator ||
            tpf.denominator != req_rate_numerator) {
            fprintf(stderr,
                   "the driver changed the time per frame from "
                   "%d/%d to %d/%d\n",
                   req_rate_denominator, req_rate_numerator,
                   tpf.numerator, tpf.denominator);
        }
    } else {
        fprintf(stderr, "the driver does not allow to change time per frame\n");
    }


    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (true) {

        fmt.fmt.pix.width       = req_width; //replace
        fmt.fmt.pix.height      = req_height; //replace
        fmt.fmt.pix.pixelformat = req_format; //replace
        fmt.fmt.pix.field       = V4L2_FIELD_ANY;

        if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
                errno_exit("VIDIOC_S_FMT");

        /* Note VIDIOC_S_FMT may change width and height. */
    } else {
        /* Preserve original settings as set by v4l2-ctl for example */
        if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
                errno_exit("VIDIOC_G_FMT");
    }

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
            fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
            fmt.fmt.pix.sizeimage = min;

    // Last step, init buffers
    init_mmap();
}

static void close_device(void)
{
    if (-1 == close(fd))
        errno_exit("close");

    fd = -1;
}

static void open_device(void)
{
    struct stat st;

    if (-1 == stat(dev_name, &st)) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                 dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "%s is no device\n", dev_name);
        exit(EXIT_FAILURE);
    }

    fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == fd) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
                 dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

#define TEST_COUNT 3
const char testfiles[TEST_COUNT] = { "test/1.yuv", "test/2.yuv", "test/3.yuv" };

char* images[TEST_COUNT];

int main(int argc, char **argv)
{
    req_format = V4L2_PIX_FMT_NV12;
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
        images[i] = calloc(st.st_size, 1);
        int r = read(fd, images[i], st.st_size);
        if (r < st.st_size) {
            fprintf(stderr, "short read of input file");
            exit(-1);
        }
            
    }
    open_device();
    init_device();

    std::string pattern = "video-capXXXXXX.avi";
    char pattern_copy[pattern.length() + 1];
    memcpy(pattern_copy, pattern.c_str(), pattern.length() + 1);
    int fd_temp;
    if ((fd_temp = mkstemps(pattern_copy, 4)) == -1) {
        close(fd_temp);
    }

    init_avi(pattern_copy, req_width, req_height, 30, req_format == V4L2_PIX_FMT_YUYV ?
             YUYV : NV12);

    start_capturing();
    mainloop();
    stop_capturing();
    uninit_device();
    close_device();
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
