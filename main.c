#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <png.h>
#include <jpeglib.h>
#include <webp/decode.h>

#define MODE_STRETCH 0
#define MODE_FILL    1
#define MODE_FOCUS   2

typedef struct { unsigned char *px; int w, h; } Image;

static Image load_png(const char *path) {
    Image img = {0};
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "xback: cannot open %s\n", path); return img; }
    png_structp ps = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   pi = png_create_info_struct(ps);
    if (setjmp(png_jmpbuf(ps))) { fclose(f); return img; }
    png_init_io(ps, f);
    png_read_info(ps, pi);
    int w = png_get_image_width(ps, pi), h = png_get_image_height(ps, pi);
    png_set_strip_16(ps); png_set_packing(ps);
    png_set_expand(ps);   png_set_filler(ps, 0xFF, PNG_FILLER_AFTER);
    png_set_gray_to_rgb(ps);
    png_read_update_info(ps, pi);
    img.px = malloc(w * h * 4); img.w = w; img.h = h;
    png_bytep *rows = malloc(h * sizeof(png_bytep));
    for (int i = 0; i < h; i++) rows[i] = img.px + i * w * 4;
    png_read_image(ps, rows);
    free(rows); png_destroy_read_struct(&ps, &pi, NULL); fclose(f);
    return img;
}
static Image load_jpg(const char *path) {
    Image img = {0};
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "xback: cannot open %s\n", path); return img; }
    struct jpeg_decompress_struct c; struct jpeg_error_mgr e;
    c.err = jpeg_std_error(&e);
    jpeg_create_decompress(&c); jpeg_stdio_src(&c, f);
    jpeg_read_header(&c, TRUE); c.out_color_space = JCS_RGB;
    jpeg_start_decompress(&c);
    img.w = c.output_width; img.h = c.output_height;
    img.px = malloc(img.w * img.h * 4);
    unsigned char *tmp = malloc(img.w * 3);
    while ((int)c.output_scanline < img.h) {
        jpeg_read_scanlines(&c, &tmp, 1);
        unsigned char *row = img.px + (c.output_scanline - 1) * img.w * 4;
        for (int i = 0; i < img.w; i++) {
            row[i*4+0]=tmp[i*3+0]; row[i*4+1]=tmp[i*3+1];
            row[i*4+2]=tmp[i*3+2]; row[i*4+3]=0xFF;
        }
    }
    free(tmp); jpeg_finish_decompress(&c); jpeg_destroy_decompress(&c); fclose(f);
    return img;
}
static Image load_webp(const char *path) {
    Image img = {0};
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "xback: cannot open %s\n", path); return img; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    unsigned char *buf = malloc(sz);
    fread(buf, 1, sz, f); fclose(f);
    img.px = WebPDecodeRGBA(buf, sz, &img.w, &img.h);
    free(buf);
    return img;
}
static Image load_image(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "xback: cannot open %s\n", path); return (Image){0}; }
    unsigned char magic[12]; fread(magic, 1, 12, f); fclose(f);
    if (magic[0]==0x89 && magic[1]=='P')                       return load_png(path);
    if (magic[0]==0xFF && magic[1]==0xD8)                      return load_jpg(path);
    if (!memcmp(magic,"RIFF",4) && !memcmp(magic+8,"WEBP",4)) return load_webp(path);
    fprintf(stderr, "xback: unsupported format\n");
    return (Image){0};
}
static void blit(Image *src, unsigned char *dst, int dx, int dy, int dw, int dh, int canw, int canh) {
    for (int y = 0; y < dh; y++) {
        int py = dy + y;
        if (py < 0 || py >= canh) continue;
        int sy = y * src->h / dh;
        if (sy >= src->h) sy = src->h - 1;
        for (int x = 0; x < dw; x++) {
            int px = dx + x;
            if (px < 0 || px >= canw) continue;
            int sx = x * src->w / dw;
            if (sx >= src->w) sx = src->w - 1;
            unsigned char *s = src->px + (sy * src->w + sx) * 4;
            unsigned char *d = dst + (py * canw + px) * 4;
            d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d[3]=s[3];
        }
    }
}
static void scale_to(Image *src, unsigned char *dst, int ox, int oy, int tw, int th,
                     int canw, int canh, int mode) {
    if (mode == MODE_STRETCH) { blit(src, dst, ox, oy, tw, th, canw, canh); return; }
    float sr = (float)src->w / src->h, tr = (float)tw / th;
    int sw, sh, bx = ox, by = oy;
    if (mode == MODE_FILL) {
        if (sr > tr) { sh = th; sw = (int)(sh * sr); bx = ox - (sw - tw) / 2; }
        else         { sw = tw; sh = (int)(sw / sr); by = oy - (sh - th) / 2; }
    } else {
        if (sr > tr) { sw = tw; sh = (int)(sw / sr); by = oy + (th - sh) / 2; }
        else         { sh = th; sw = (int)(sh * sr); bx = ox + (tw - sw) / 2; }
    }
    blit(src, dst, bx, by, sw, sh, canw, canh);
}
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "\033[97m   _  __ ____             __  \n"
            "  | |/ // __ )____ ______/ /__\n"
            "  |   // __  / __ `/ ___/ //_/\n"
            " /   |/ /_/ / /_/ / /__/ ,<   \n"
            "/_/|_/_____/\\__,_/\\___/_/|_|  \033[0m\n"
            "\n"
            "  \033[1mXwallpaper if it sucked less\033[0m\n"
            "\n"
            "  Usage:\n"
            "    xback <monitor> <image> [mode]\n"
            "\n"
            "  Modes:\n"
            "    --fill      Scale and crop to fill monitor (default)\n"
            "    --stretch   Stretch to fit, aspect ratio be damned\n"
            "    --focus     Fit entire image, letterboxed\n"
            "\n"
            "  Example:\n"
            "    xback eDP-1 ~/walls/mywall.jpg --fill\n"
            "\n");
        return 1;
    }
    const char *monitor = argv[1], *path = argv[2];
    int mode = MODE_FILL;
    if (argc >= 4) {
        if      (!strcmp(argv[3],"--stretch")) mode = MODE_STRETCH;
        else if (!strcmp(argv[3],"--focus"))   mode = MODE_FOCUS;
    }
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "xback: cannot open display\n"); return 1; }
    Window root = DefaultRootWindow(dpy);
    int sw = DisplayWidth(dpy, DefaultScreen(dpy)), sh = DisplayHeight(dpy, DefaultScreen(dpy));
    int ox = 0, oy = 0, mw = sw, mh = sh, found = 0;
    XRRScreenResources *res = XRRGetScreenResources(dpy, root);
    if (res) {
        for (int i = 0; i < res->noutput && !found; i++) {
            XRROutputInfo *oi = XRRGetOutputInfo(dpy, res, res->outputs[i]);
            if (oi->connection == RR_Connected && !strcmp(oi->name, monitor)) {
                XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, oi->crtc);
                ox=ci->x; oy=ci->y; mw=ci->width; mh=ci->height; found=1;
                XRRFreeCrtcInfo(ci);
            }
            XRRFreeOutputInfo(oi);
        }
        XRRFreeScreenResources(res);
    }
    if (!found) fprintf(stderr, "xback: monitor '%s' not found, using full screen\n", monitor);
    Image img = load_image(path);
    if (!img.px) { XCloseDisplay(dpy); return 1; }
    fprintf(stderr, "xback: loaded %dx%d\n", img.w, img.h);
    unsigned char *canvas = calloc(sw * sh, 4);
    scale_to(&img, canvas, ox, oy, mw, mh, sw, sh, mode);
    free(img.px);
    int depth = DefaultDepth(dpy, DefaultScreen(dpy));
    Visual *vis = DefaultVisual(dpy, DefaultScreen(dpy));
    int bpp = (depth > 16) ? 4 : 2;
    int stride = sw * bpp;
    unsigned char *xbuf = malloc(sw * sh * bpp);
    for (int i = 0; i < sw * sh; i++) {
        unsigned int r=canvas[i*4+0], g=canvas[i*4+1], b=canvas[i*4+2];
        if (bpp == 4) {
            xbuf[i*4+0]=b; xbuf[i*4+1]=g; xbuf[i*4+2]=r; xbuf[i*4+3]=0;
        } else {
            unsigned short px = ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
            xbuf[i*2+0]=px&0xFF; xbuf[i*2+1]=(px>>8)&0xFF;
        }
    }
    free(canvas); canvas = NULL;
    Pixmap pmap = XCreatePixmap(dpy, root, sw, sh, depth);
    GC gc = XCreateGC(dpy, pmap, 0, NULL);
    XImage *xi = XCreateImage(dpy, vis, depth, ZPixmap, 0, (char *)xbuf, sw, sh, bpp*8, stride);
    XPutImage(dpy, pmap, gc, xi, 0, 0, 0, 0, sw, sh);
    XSetWindowBackgroundPixmap(dpy, root, pmap);
    XClearWindow(dpy, root);
    Atom prop = XInternAtom(dpy, "_XROOTPMAP_ID", False);
    Atom erop = XInternAtom(dpy, "ESETROOT_PMAP_ID", False);
    XChangeProperty(dpy, root, prop, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pmap, 1);
    XChangeProperty(dpy, root, erop, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pmap, 1);
    XSetCloseDownMode(dpy, RetainPermanent);
    XFlush(dpy);
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);
    return 0;
}
