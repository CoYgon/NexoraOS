#include <stddef.h>
#include <stdint.h>
#include "heap.h"
#include "assets.h"
#include "framebuffer.h"

#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_SIMD
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(sz) nexora_malloc(sz)
#define STBI_REALLOC(p,sz) nexora_realloc((p),(sz))
#define STBI_FREE(p) nexora_free(p)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static double nx_acos(double x);
static double nx_cos(double x);
static double nx_cuberoot(double x);
static double nx_fmod(double x, double y);
static double nx_sqrt(double x);
static int nx_ifloor(double x);
static int nx_iceil(double x);
#define STBTT_pow(x,y) nx_cuberoot((x))
#define STBTT_cos(x) nx_cos((x))
#define STBTT_acos(x) nx_acos((x))
#define STBTT_fmod(x,y) nx_fmod((x),(y))
#define STBTT_fabs(x) __builtin_fabs(x)
#define STBTT_ifloor(x) nx_ifloor((x))
#define STBTT_iceil(x) nx_iceil((x))
#define STBTT_sqrt(x) nx_sqrt((x))
#define STBTT_malloc(x,u) ((void)(u), nexora_malloc(x))
#define STBTT_free(x,u) ((void)(u), nexora_free(x))
#define STBTT_assert(x) ((void)0)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/* Minimal freestanding C runtime primitives used by the vendored decoders. */
void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst; const uint8_t *s = src;
    for (size_t i=0;i<n;i++) d[i]=s[i];
    return dst;
}
void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d=dst; const uint8_t *s=src;
    if (d < s) for (size_t i=0;i<n;i++) d[i]=s[i];
    else if (d > s) for (size_t i=n;i;i--) d[i-1]=s[i-1];
    return dst;
}
void *memset(void *dst, int c, size_t n) { uint8_t *d=dst; for(size_t i=0;i<n;i++) d[i]=(uint8_t)c; return dst; }
int memcmp(const void *a,const void *b,size_t n) { const uint8_t *x=a,*y=b; for(size_t i=0;i<n;i++) if(x[i]!=y[i]) return x[i]<y[i]?-1:1; return 0; }
size_t strlen(const char *s) { size_t n=0; while(s[n]) n++; return n; }

int assets_image_decode(const uint8_t *data, size_t size, nexoraos_image_t *out) {
    if (!out) return 0;
    out->width=out->height=out->channels=0; out->pixels=0;
    if (!data || !size || size > 0x7fffffffU) return 0;
    int w=0,h=0,channels=0;
    if (!stbi_info_from_memory(data,(int)size,&w,&h,&channels) || w<=0 || h<=0)
        return 0;
    /* Reject oversized images before decoding, not after the heap was consumed. */
    if ((uint64_t)w*(uint64_t)h > (uint64_t)(heap_capacity()/8u)) return 0;
    stbi_uc *pixels=stbi_load_from_memory(data,(int)size,&w,&h,&channels,4);
    if (!pixels || w<=0 || h<=0) { if(pixels) stbi_image_free(pixels); return 0; }
    out->width=(uint32_t)w; out->height=(uint32_t)h; out->channels=4; out->pixels=pixels;
    return 1;
}
void assets_image_release(nexoraos_image_t *image) {
    if (!image) return;
    if (image->pixels) stbi_image_free(image->pixels);
    image->pixels=0; image->width=image->height=image->channels=0;
}
void assets_image_draw(const nexoraos_image_t *image, int x, int y) {
    if (!image || !image->pixels || image->channels != 4 || !fb_available() ||
        (uint64_t)image->width * (uint64_t)image->height > (uint64_t)(heap_capacity()/8u)) return;
    uint64_t sw=fb_width(), sh=fb_height();
    for (uint32_t iy=0; iy<image->height; ++iy) {
        int64_t dy=(int64_t)y+iy; if(dy<0 || (uint64_t)dy>=sh) continue;
        for(uint32_t ix=0; ix<image->width; ++ix) {
            int64_t dx=(int64_t)x+ix; if(dx<0 || (uint64_t)dx>=sw) continue;
            const uint8_t *p=image->pixels+(((size_t)iy*image->width+ix)*4u);
            uint32_t a=p[3]; if(!a) continue;
            uint32_t src=((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
            if(a==255) fb_pixel((uint64_t)dx,(uint64_t)dy,src);
            else {
                uint32_t old=fb_read_pixel((uint64_t)dx,(uint64_t)dy);
                uint32_t r=(((src>>16)&255u)*a+((old>>16)&255u)*(255u-a))/255u;
                uint32_t g=(((src>>8)&255u)*a+((old>>8)&255u)*(255u-a))/255u;
                uint32_t b=((src&255u)*a+(old&255u)*(255u-a))/255u;
                fb_pixel((uint64_t)dx,(uint64_t)dy,(r<<16)|(g<<8)|b);
            }
        }
    }
}
void assets_image_draw_scaled(const nexoraos_image_t *image, int x, int y, uint32_t width, uint32_t height) {
    if (!image || !image->pixels || image->channels != 4 || !image->width || !image->height ||
        !width || !height || !fb_available() ||
        (uint64_t)image->width * image->height > (uint64_t)(heap_capacity()/8u)) return;
    uint64_t sw=fb_width(), sh=fb_height();
    int64_t x2=(int64_t)x+width, y2=(int64_t)y+height;
    int64_t left=x<0?0:x, top=y<0?0:y;
    if (x2>(int64_t)sw) x2=(int64_t)sw;
    if (y2>(int64_t)sh) y2=(int64_t)sh;
    int cx,cy,cw,ch;
    if (fb_get_clip(&cx,&cy,&cw,&ch)) {
        if (left<cx) left=cx;
        if (top<cy) top=cy;
        if (x2>(int64_t)cx+cw) x2=(int64_t)cx+cw;
        if (y2>(int64_t)cy+ch) y2=(int64_t)cy+ch;
    }
    if (left>=x2 || top>=y2) return;
    for (int64_t dy=top; dy<y2; ++dy) {
        uint32_t sy=(uint32_t)(((uint64_t)(dy-y)*image->height)/height);
        if (sy>=image->height) sy=image->height-1;
        for (int64_t dx=left; dx<x2; ++dx) {
            uint32_t sx=(uint32_t)(((uint64_t)(dx-x)*image->width)/width);
            if (sx>=image->width) sx=image->width-1;
            const uint8_t *p=image->pixels+(((size_t)sy*image->width+sx)*4u);
            uint32_t a=p[3]; if(!a) continue;
            uint32_t src=((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
            if(a==255) fb_pixel((uint64_t)dx,(uint64_t)dy,src);
            else {
                uint32_t old=fb_read_pixel((uint64_t)dx,(uint64_t)dy);
                uint32_t r=(((src>>16)&255u)*a+((old>>16)&255u)*(255u-a))/255u;
                uint32_t g=(((src>>8)&255u)*a+((old>>8)&255u)*(255u-a))/255u;
                uint32_t b=((src&255u)*a+(old&255u)*(255u-a))/255u;
                fb_pixel((uint64_t)dx,(uint64_t)dy,(r<<16)|(g<<8)|b);
            }
        }
    }
}

static uint16_t read_be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t read_be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int font_directory_valid(const uint8_t *data, size_t size, uint32_t *font_offset) {
    if (!data || size < 12 || !font_offset) return 0;
    uint32_t off=0, signature=read_be32(data);
    if (signature == 0x74746366u) { /* TTC: use the first face only */
        if (size < 16 || read_be32(data+8) == 0) return 0;
        off=read_be32(data+12);
    } else if (signature != 0x00010000u && signature != 0x4F54544Fu &&
               signature != 0x74727565u && signature != 0x74797031u) return 0;
    if ((uint64_t)off + 12u > size) return 0;
    uint16_t tables=read_be16(data+off+4);
    uint64_t dir_end=(uint64_t)off+12u+(uint64_t)tables*16u;
    if (!tables || dir_end > size) return 0;
    for (uint16_t i=0;i<tables;i++) {
        const uint8_t *record=data+off+12u+(size_t)i*16u;
        uint32_t table_off=read_be32(record+8), table_len=read_be32(record+12);
        if ((uint64_t)table_off+(uint64_t)table_len > size) return 0;
    }
    *font_offset=off;
    return 1;
}

int assets_font_draw_codepoint(const nexoraos_font_t *font, uint32_t cp,
                               int x, int baseline_y, uint32_t color) {
    if (!font || !font->data || font->size > 0x7fffffffU || font->size < 12 ||
        font->pixel_height <= 0.0f || font->pixel_height > 256.0f ||
        cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu) || !fb_available()) return -1;
    uint32_t font_offset=0;
    if (!font_directory_valid(font->data,font->size,&font_offset)) return -1;
    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info,font->data,(int)font_offset)) return -1;
    float scale=stbtt_ScaleForPixelHeight(&info,font->pixel_height);
    int advance_width=0, left_bearing=0;
    stbtt_GetCodepointHMetrics(&info,(int)cp,&advance_width,&left_bearing);
    int advance=(int)(advance_width*scale+0.5f);
    int w=0,h=0,xoff=0,yoff=0;
    unsigned char *bitmap=stbtt_GetCodepointBitmap(&info,scale,scale,(int)cp,&w,&h,&xoff,&yoff);
    if (!bitmap) return advance;
    uint64_t sw=fb_width(),sh=fb_height();
    for(int gy=0;gy<h;gy++) {
        int64_t dy=(int64_t)baseline_y+yoff+gy; if(dy<0||(uint64_t)dy>=sh) continue;
        for(int gx=0;gx<w;gx++) {
            int64_t dx=(int64_t)x+xoff+gx; if(dx<0||(uint64_t)dx>=sw) continue;
            uint32_t a=bitmap[(size_t)gy*w+gx]; if(!a) continue;
            if(a==255) fb_pixel((uint64_t)dx,(uint64_t)dy,color);
            else {
                uint32_t old=fb_read_pixel((uint64_t)dx,(uint64_t)dy);
                uint32_t r=(((color>>16)&255u)*a+((old>>16)&255u)*(255u-a))/255u;
                uint32_t g=(((color>>8)&255u)*a+((old>>8)&255u)*(255u-a))/255u;
                uint32_t b=((color&255u)*a+(old&255u)*(255u-a))/255u;
                fb_pixel((uint64_t)dx,(uint64_t)dy,(r<<16)|(g<<8)|b);
            }
        }
    }
    nexora_free(bitmap);
    return advance;
}

int strcmp(const char *a,const char *b) { while(*a && *a==*b){a++;b++;} return (unsigned char)*a-(unsigned char)*b; }
int strncmp(const char *a,const char *b,size_t n) { for(size_t i=0;i<n;i++){ unsigned char x=a[i],y=b[i]; if(x!=y)return x-y; if(!x)return 0;} return 0; }
int abs(int x) { return x<0 ? -x : x; }
int atoi(const char *s) { int sign=1,v=0; while(*s==' '||*s=='\t')s++; if(*s=='-'||*s=='+'){if(*s++=='-')sign=-1;} while(*s>='0'&&*s<='9'){ if(v>214748364) break; v=v*10+(*s++-'0'); } return sign*v; }
long strtol(const char *s,char **end,int base) { long v=0; int sign=1; while(*s==' '||*s=='\t')s++; if(*s=='-'){sign=-1;s++;} if(base<2||base>36)base=10; while(*s){int d=(*s>='0'&&*s<='9')?*s-'0':((*s>='a'&&*s<='z')?*s-'a'+10:((*s>='A'&&*s<='Z')?*s-'A'+10:-1)); if(d<0||d>=base)break; v=v*base+d;s++;} if(end)*end=(char*)s; return sign*v; }

static double nx_cos(double x) { const double pi=3.14159265358979323846, tau=6.28318530717958647692; while(x>pi)x-=tau; while(x< -pi)x+=tau; double x2=x*x; return 1.0-x2/2.0+x2*x2/24.0-x2*x2*x2/720.0+x2*x2*x2*x2/40320.0; }
static double nx_acos(double x) { if(x>1.0)x=1.0; if(x< -1.0)x=-1.0; double ax=x<0?-x:x; double r=(-0.0187293*ax+0.0742610)*ax-0.2121144; r=(r*ax+1.5707288)*nx_sqrt(1.0-ax); return x<0?3.14159265358979323846-r:r; }
static double nx_cuberoot(double x) { if(x==0.0)return 0.0; int neg=x<0.0; if(neg)x=-x; double r=x>1.0?x:1.0; for(int i=0;i<24;i++)r=(2.0*r+x/(r*r))/3.0; return neg?-r:r; }

static double nx_fmod(double x,double y) { if(y==0.0)return 0.0; long long q=(long long)(x/y); return x-(double)q*y; }

static double nx_sqrt(double x) { double r; if(x<=0.0)return 0.0; __asm__ volatile("sqrtsd %1, %0" : "=x"(r) : "x"(x)); return r; }

static int nx_ifloor(double x) { int i=(int)x; return x<(double)i?i-1:i; }
static int nx_iceil(double x) { int i=(int)x; return x>(double)i?i+1:i; }
