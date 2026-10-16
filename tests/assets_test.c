#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "assets.h"
#include "heap.h"

static uint32_t pixels[10][10];
int fb_available(void) { return 1; }
int fb_get_clip(int *x,int *y,int *w,int *h) { (void)x;(void)y;(void)w;(void)h; return 0; }
uint64_t fb_width(void) { return 10; }
uint64_t fb_height(void) { return 10; }
void fb_pixel(uint64_t x,uint64_t y,uint32_t c) { if(x<10&&y<10)pixels[y][x]=c; }
uint32_t fb_read_pixel(uint64_t x,uint64_t y) { return x<10&&y<10?pixels[y][x]:0; }

static const uint8_t png[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0xf4, 0x22, 0x7f, 0x8a, 0x00, 0x00, 0x00, 0x0e, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0x00, 0x42, 0x0d, 0x00, 0x0f, 0x7a, 0x03, 0x7e, 0x77, 0xe9, 0x7f, 0x97, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82 };
int main(void) {
    heap_init();
    size_t initial=heap_used();
    void *a=nexora_malloc(128), *b=nexora_malloc(256);
    assert(a && b && ((uintptr_t)a % 16u)==0);
    for(int i=0;i<128;i++)((uint8_t*)a)[i]=(uint8_t)i;
    a=nexora_realloc(a,192); assert(a);
    for(int i=0;i<128;i++)assert(((uint8_t*)a)[i]==(uint8_t)i);
    nexora_free(b); nexora_free(a); assert(heap_used()==initial);
    assert(nexora_malloc(heap_capacity())==0);

    nexoraos_image_t image;
    assert(assets_image_decode(png,sizeof(png),&image));
    assert(image.width==2 && image.height==1 && image.channels==4 && image.pixels);
    assert(image.pixels[0]==255 && image.pixels[1]==0 && image.pixels[2]==0 && image.pixels[3]==255);
    assets_image_draw(&image,1,1);
    assert(pixels[1][1]==0xFF0000u);
    assert(pixels[1][2]==0x000080u);
    assets_image_draw_scaled(&image,0,0,4,2);
    assert(pixels[0][0]==0xFF0000u && pixels[0][3]==0x000080u);
    assert(pixels[1][0]==0xFF0000u && pixels[1][3]==0x000080u);
    assets_image_release(&image);
    assert(!image.pixels && image.width==0);
    assert(!assets_image_decode((const uint8_t*)"bad",3,&image));

    /* Exercise the actual bundled wallpaper and TTF used by the desktop. */
    FILE *f=fopen("assets/WALLPAPER.PNG","rb");
    assert(f); assert(fseek(f,0,SEEK_END)==0); long file_size=ftell(f); assert(file_size>0);
    assert(fseek(f,0,SEEK_SET)==0); uint8_t *file_data=(uint8_t*)malloc((size_t)file_size); assert(file_data);
    assert(fread(file_data,1,(size_t)file_size,f)==(size_t)file_size); fclose(f);
    assert(assets_image_decode(file_data,(size_t)file_size,&image));
    assert(image.width==480 && image.height==300 && image.channels==4);
    assets_image_release(&image); free(file_data);

    f=fopen("assets/DejaVuSansMono.ttf","rb");
    assert(f); assert(fseek(f,0,SEEK_END)==0); file_size=ftell(f); assert(file_size>100000 && file_size<384*1024);
    assert(fseek(f,0,SEEK_SET)==0); file_data=(uint8_t*)malloc((size_t)file_size); assert(file_data);
    assert(fread(file_data,1,(size_t)file_size,f)==(size_t)file_size); fclose(f);
    nexoraos_font_t font={file_data,(size_t)file_size,12.0f};
    assert(assets_font_draw_codepoint(&font,'A',-100,-100,0xFFFFFFu)>0);
    free(file_data);
    puts("NexoraOS heap + stb_image + bundled TTF asset tests passed");
    return 0;
}
