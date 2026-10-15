#include "blockdev.h"
#include "fat32.h"
#include "emrefs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOL_SECTORS 66581u
#define PARTITION_START 2048u
#define SECTORS (PARTITION_START + VOL_SECTORS)
static unsigned char *image;
static int memory_read(void *ctx,uint64_t lba,uint32_t count,void *buffer){
    (void)ctx;if(lba>=SECTORS||(uint64_t)count>SECTORS-lba)return 0;
    memcpy(buffer,image+(size_t)lba*512,(size_t)count*512);return 1;
}
static int memory_write(void *ctx,uint64_t lba,uint32_t count,const void *buffer){
    (void)ctx;if(lba>=SECTORS||(uint64_t)count>SECTORS-lba)return 0;
    memcpy(image+(size_t)lba*512,buffer,(size_t)count*512);return 1;
}
static void put16(unsigned char*p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void put32(unsigned char*p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static unsigned crc32(const unsigned char*p,size_t n){unsigned c=0xFFFFFFFFu;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^(0xEDB88320u&-(int)(c&1));}return ~c;}
int main(void){
    image=(unsigned char*)calloc(SECTORS,512);assert(image);
    unsigned char *mbr=image; mbr[446+4]=0x0C; put32(mbr+446+8,PARTITION_START); put32(mbr+446+12,VOL_SECTORS); mbr[510]=0x55;mbr[511]=0xAA;
    unsigned char *b=image+(size_t)PARTITION_START*512;put16(b+11,512);b[13]=1;put16(b+14,32);b[16]=2;put16(b+17,0);put16(b+19,0);put16(b+22,0);put32(b+32,VOL_SECTORS);put32(b+36,512);put32(b+44,2);b[510]=0x55;b[511]=0xAA;
    unsigned char *fat=image+(size_t)(PARTITION_START+32)*512;put32(fat+2*4,0x0FFFFFFF);put32(fat+3*4,0x0FFFFFFF);put32(fat+4*4,0x0FFFFFFF);put32(fat+5*4,0x0FFFFFFF);
    unsigned char *root=image+(size_t)(PARTITION_START+32+1024)*512;memcpy(root,"HELLO   TXT",11);root[11]=0x20;put16(root+26,3);put32(root+28,5);
    memcpy(root+32,"SUBDIR     ",11);root[32+11]=0x10;put16(root+32+26,4);root[64]=0;
    unsigned char *sub=image+(size_t)(PARTITION_START+32+1024+2)*512;memcpy(sub,"INNER   TXT",11);sub[11]=0x20;put16(sub+26,5);put32(sub+28,5);sub[32]=0;
    memcpy(image+(size_t)(PARTITION_START+32+1024+1)*512,"hello",5);
    memcpy(image+(size_t)(PARTITION_START+32+1024+3)*512,"world",5);
    blockdev_init();assert(blockdev_register("test-memory",SECTORS,memory_read,0));fat32_init();assert(fat32_auto_mount());assert(fat32_is_mounted());assert(fat32_partition_lba()==PARTITION_START);
    char out[128];assert(fat32_list_root(out,sizeof(out))==2);assert(strstr(out,"HELLO.TXT"));unsigned bytes=0;assert(fat32_read_root_file("hello.txt",out,sizeof(out),&bytes)==0);assert(bytes==5&&strcmp(out,"hello")==0);
    assert(fat32_list_path("/SUBDIR",out,sizeof(out))==1&&strstr(out,"INNER.TXT"));
    assert(fat32_read_file("subdir/inner.txt",out,sizeof(out),&bytes)==0&&bytes==5&&strcmp(out,"world")==0);
    assert(!blockdev_read(SECTORS,1,out));
    fat32_init(); b[12]=0; assert(!fat32_mount(PARTITION_START,VOL_SECTORS));
    unsigned char sb[512]={0};memcpy(sb,"EMREFS1",7);put16(sb+8,1);put16(sb+10,64);put32(sb+12,512);put32(sb+16,100);put32(sb+24,1);put32(sb+32,4);put32(sb+40,5);put32(sb+60,crc32(sb,60));assert(emrefs_validate_superblock(sb,100));sb[60]^=1;assert(!emrefs_validate_superblock(sb,100));
    /* Exercise writable EmreFS only on an explicit in-memory disk backend. */
    blockdev_init();assert(blockdev_register_rw("test-memory-rw",SECTORS,memory_read,memory_write,0));emrefs_init();
    assert(emrefs_format(1000,128));assert(emrefs_is_mounted()&&emrefs_is_writable());
    assert(emrefs_mkdir("docs"));const char msg[]="EmreFS works";assert(emrefs_write_file("note.txt",msg,sizeof(msg)));
    char listing[256];assert(emrefs_list_root(listing,sizeof(listing))==2);assert(strstr(listing,"docs/")&&strstr(listing,"note.txt"));
    char readback[64];uint32_t readbytes=0;assert(emrefs_read_file("note.txt",readback,sizeof(readback),&readbytes));assert(readbytes==sizeof(msg)&&memcmp(readback,msg,sizeof(msg))==0);
    const char updated[]="updated data";assert(emrefs_write_file("note.txt",updated,sizeof(updated)));
    emrefs_init();assert(emrefs_mount(1000,128));assert(emrefs_read_file("note.txt",readback,sizeof(readback),&readbytes));assert(readbytes==sizeof(updated)&&memcmp(readback,updated,sizeof(updated))==0);
    /* Read-only devices can mount, but cannot format or mutate. */
    blockdev_init();assert(blockdev_register("test-readonly",SECTORS,memory_read,0));emrefs_init();assert(emrefs_mount(1000,128));assert(!emrefs_is_writable());assert(!emrefs_write_file("bad.txt","x",1));
    free(image);puts("EmreOS V6.1 storage tests passed (FAT32 subdirectories + EmreFS format/list/read/write on memory disk)");return 0;
}
