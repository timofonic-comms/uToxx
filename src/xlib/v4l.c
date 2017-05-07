#include "main.h"

#include "../macros.h"

#include "../av/video.h" // video super globals

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

int utox_v4l_fd = -1;

#include <sys/mman.h>
#if defined(__linux__) || defined(__FreeBSD__) //FreeBSD will have the proper includes after installing v4l
#include <linux/videodev2.h>
#else //OpenBSD
#include <sys/videoio.h>
#endif

#ifndef NO_V4LCONVERT
#include <libv4lconvert.h>
#endif

#define CLEAR(x) memset(&(x), 0, sizeof(x))

static int xioctl(int fh, unsigned long request, void *arg) {
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

struct buffer {
    void * start;
    size_t length;
};

static struct buffer *buffers;
static uint32_t       n_buffers;

#ifndef NO_V4LCONVERT
static struct v4lconvert_data *v4lconvert_data;
#endif

static struct v4l2_format fmt, dest_fmt = {
    //.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .fmt = {
        .pix = {
            .pixelformat = V4L2_PIX_FMT_YUV420,
            //.field = V4L2_FIELD_NONE,
        },
    },
};

bool v4l_init(char *dev_name) {
    utox_v4l_fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == utox_v4l_fd) {
        return 0;
    }

    struct v4l2_capability cap;
    struct v4l2_cropcap    cropcap;
    struct v4l2_crop       crop;
    unsigned int           min;

    if (-1 == xioctl(utox_v4l_fd, VIDIOC_QUERYCAP, &cap)) {
        return 0;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        return 0;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        return 0;
    }

    /* Select video input, video standard and tune here. */
    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(utox_v4l_fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c    = cropcap.defrect; /* reset to default */

        if (-1 == xioctl(utox_v4l_fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
            case EINVAL:
                /* Cropping not supported. */
                break;
            default:
                /* Errors ignored. */
                break;
            }
        }
    }

#ifndef NO_V4LCONVERT
    v4lconvert_data = v4lconvert_create(utox_v4l_fd);
#endif

    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl(utox_v4l_fd, VIDIOC_G_FMT, &fmt)) {
        return 0;
    }

    video_width             = fmt.fmt.pix.width;
    video_height            = fmt.fmt.pix.height;
    dest_fmt.fmt.pix.width  = fmt.fmt.pix.width;
    dest_fmt.fmt.pix.height = fmt.fmt.pix.height;


    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min) {
        fmt.fmt.pix.bytesperline = min;
    }
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min) {
        fmt.fmt.pix.sizeimage = min;
    }


    /* part 3*/
    // uint32_t buffer_size = fmt.fmt.pix.sizeimage;
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP; // V4L2_MEMORY_USERPTR;

    if (-1 == xioctl(utox_v4l_fd, VIDIOC_REQBUFS, &req)) {
        return 0;
    }

    if (req.count < 2) {
        exit(1);
    }

    buffers = calloc(req.count, sizeof(*buffers));

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = n_buffers;

        if (-1 == xioctl(utox_v4l_fd, VIDIOC_QUERYBUF, &buf)) {
            return 0;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start  = mmap(NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */,
                                        MAP_SHARED /* recommended */, utox_v4l_fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start) {
            return 0;
        }
    }

    return 1;
}

void v4l_close(void) {
    size_t i;
    for (i = 0; i < n_buffers; ++i) {
        if (-1 == munmap(buffers[i].start, buffers[i].length)) {
        }
    }

    close(utox_v4l_fd);
}

bool v4l_startread(void) {
    size_t i;
    enum v4l2_buf_type type;

    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP; // V4L2_MEMORY_USERPTR;
        buf.index  = i;
        // buf.m.userptr = (unsigned long)buffers[i].start;
        // buf.length = buffers[i].length;

        if (-1 == xioctl(utox_v4l_fd, VIDIOC_QBUF, &buf)) {
            return 0;
        }
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(utox_v4l_fd, VIDIOC_STREAMON, &type)) {
        return 0;
    }

    return 1;
}

bool v4l_endread(void) {
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(utox_v4l_fd, VIDIOC_STREAMOFF, &type)) {
        return 0;
    }

    return 1;
}

int v4l_getframe(uint8_t *y, uint8_t *UNUSED(u), uint8_t *UNUSED(v), uint16_t width, uint16_t height) {
    if (width != video_width || height != video_height) {
        return 0;
    }

    struct v4l2_buffer buf;
    // unsigned int i;

    CLEAR(buf);

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP; // V4L2_MEMORY_USERPTR;

    if (-1 == ioctl(utox_v4l_fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
        case EINTR:
        case EAGAIN:
            return 0;

        case EIO: /* Could ignore EIO, see spec. */
        default:
            return -1;
        }
    }

    void *data = (void *)buffers[buf.index].start; // length = buf.bytesused //(void*)buf.m.userptr

/* assumes planes are continuous memory */
#ifndef NO_V4LCONVERT
    int result = v4lconvert_convert(v4lconvert_data, &fmt, &dest_fmt, data, buf.bytesused, y,
                                    (video_width * video_height * 3) / 2);
#else
    if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
        yuv422to420(y, u, v, data, video_width, video_height);
    }
#endif

    if (-1 == xioctl(utox_v4l_fd, VIDIOC_QBUF, &buf)) {
    }

#ifndef NO_V4LCONVERT
    return (result == -1 ? 0 : 1);
#else
    return 1;
#endif
}
