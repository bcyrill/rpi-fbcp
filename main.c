
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include <bcm_host.h>

#define FPS 15

int process() {
    DISPMANX_DISPLAY_HANDLE_T display;
    DISPMANX_MODEINFO_T display_info;
    DISPMANX_RESOURCE_HANDLE_T screen_resource;
    VC_IMAGE_TRANSFORM_T transform;
    uint32_t image_prt;
    VC_RECT_T rect1;
    int ret;
    unsigned int count = 0;
    int fbfd = 0;
    int dpitch = 0;
    int delay = 1000000 / FPS;
    char *fbp = 0;
    char *fbdev0 = "/dev/fb0";
    char *fbdev1 = "/dev/fb1";
    struct fb_var_screeninfo vinfo0;
    struct fb_var_screeninfo vinfo1;
    struct fb_fix_screeninfo finfo0;
    struct fb_fix_screeninfo finfo1;

    bcm_host_init();

    fbfd = open(fbdev0, O_RDWR);
    if (fbfd < 0) {
        syslog(LOG_ERR, "Unable to open fbdev0 display");
        return -1;
    }
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo0)) {
        syslog(LOG_ERR, "Unable to get fbdev0 fb display information");
        return -1;
    }
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo0)) {
        syslog(LOG_ERR, "Unable to get fbdev0 fb display information");
        return -1;
    }
    syslog(LOG_INFO, "fbdev0 display is %d x %d %dbps\n", vinfo0.xres, vinfo0.yres, vinfo0.bits_per_pixel);
    close(fbfd);

    fbfd = open(fbdev1, O_RDWR);
    if (fbfd < 0) {
        syslog(LOG_ERR, "Unable to open fbdev1 display %s",fbdev1);
        return -1;
    }
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo1)) {
        syslog(LOG_ERR, "Unable to get fbdev1 display information");
        return -1;
    }
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo1)) {
        syslog(LOG_ERR, "Unable to get fbdev1 display information");
        return -1;
    }
    syslog(LOG_INFO, "fbdev1 display is %d x %d %dbps\n", vinfo1.xres, vinfo1.yres, vinfo1.bits_per_pixel);

    dpitch = vinfo1.xres * vinfo1.bits_per_pixel / 8;
    screen_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, vinfo1.xres, vinfo1.yres, &image_prt);
    if (!screen_resource) {
        syslog(LOG_ERR, "Unable to create screen buffer");
        close(fbfd);
        return -1;
    }

    fbp = (char*) mmap(0, finfo1.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbp <= 0) {
        syslog(LOG_ERR, "Unable to create mamory mapping");
        close(fbfd);
        ret = vc_dispmanx_resource_delete(screen_resource);
        return -1;
    } else {
        syslog(LOG_INFO, "Starting Snapshotting with delay of %d", delay);
    }

    vc_dispmanx_rect_set(&rect1, 0, 0, vinfo1.xres, vinfo1.yres);

    ret = 0;
    while (ret == 0) {
        ret = vc_dispmanx_snapshot(display, screen_resource, 0);
        if (ret != 0) {
		count +=1;
		if ( count % 60 == 0 ) {
			syslog(LOG_ERR, "Unable to snapshot %d. (%u)", ret, count);
		}
		// for when tvservice -o has been called - check state some how?
		vc_dispmanx_resource_delete(screen_resource);
		display = vc_dispmanx_display_open(0);
		screen_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, vinfo1.xres, vinfo1.yres, &image_prt);
		sleep(1);
		ret = 0;
        } else {
	     if (count > 0 ) {
		syslog(LOG_INFO, "Snapshotting after %u attempts.",count);
		count = 0;
	     }
	     ret = vc_dispmanx_resource_read_data(screen_resource, &rect1, fbp, dpitch);
             if (ret != 0) {
                 syslog(LOG_ERR, "Unable to read data from vc %d.",ret);
             }
             usleep(delay);
        }
    }
    syslog(LOG_INFO, "Terminating...");

    munmap(fbp, finfo1.smem_len);
    close(fbfd);
    ret = vc_dispmanx_resource_delete(screen_resource);
    vc_dispmanx_display_close(display);
}

int main(int argc, char **argv) {
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("fbcp", LOG_NDELAY | LOG_PID, LOG_USER);

    return process();
}

