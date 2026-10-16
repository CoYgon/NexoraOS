#include "emrefs.h"
#include "blockdev.h"
#include <stddef.h>

#define EMREFS_ENTRY_SIZE 32u
#define EMREFS_ENTRY_FILE 1u
#define EMREFS_ENTRY_DIR 2u
#define EMREFS_MAX_SECTORS 0xFFFFFFFFu
struct emrefs_volume { int mounted; uint64_t base,total; uint64_t bitmap_lba,root_lba,data_lba; uint32_t bitmap_sectors,root_sectors,data_sectors; };
static struct emrefs_volume fs;
static uint8_t io_sector[512];
static void clear_sector(void){volatile uint8_t *p=io_sector;for(unsigned i=0;i<512;++i)p[i]=0;}
static uint16_t le16(const uint8_t*p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t le64(const uint8_t*p){return (uint64_t)le32(p)|((uint64_t)le32(p+4)<<32);}
static void put16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
static void put64(uint8_t*p,uint64_t v){put32(p,(uint32_t)v);put32(p+4,(uint32_t)(v>>32));}
static uint32_t crc32(const uint8_t *p,size_t n){uint32_t c=0xFFFFFFFFu;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^(0xEDB88320u&((uint32_t)-(int32_t)(c&1)));}return ~c;}
static uint32_t bitmap_need(uint64_t data_sectors){return (uint32_t)((data_sectors+4095u)/4096u);}
int emrefs_validate_superblock(const uint8_t *s,uint64_t device_sectors){
    if(!s||device_sectors<16)return 0;static const uint8_t magic[8]={'E','M','R','E','F','S','1',0};
    for(unsigned i=0;i<8;++i)if(s[i]!=magic[i])return 0;
    if(le16(s+8)!=1||le16(s+10)!=64||le32(s+12)!=512)return 0;
    uint64_t total=le64(s+16),meta_start=le64(s+24),meta_count=le64(s+32),data_start=le64(s+40);
    if(total<16||total>device_sectors||meta_start!=1||meta_count<2||meta_start>total||meta_count>total-meta_start||data_start<meta_start+meta_count||data_start>=total)return 0;
    if(meta_count>0xFFFFFFFFu||data_start>0xFFFFFFFFu)return 0;
    uint64_t data=total-data_start;if(bitmap_need(data)+1>meta_count)return 0;
    if(crc32(s,60)!=le32(s+60))return 0;return 1;
}
void emrefs_init(void){fs.mounted=0;fs.base=fs.total=0;fs.bitmap_lba=fs.root_lba=fs.data_lba=0;fs.bitmap_sectors=fs.root_sectors=fs.data_sectors=0;}
int emrefs_mount(uint64_t base,uint64_t sectors){
    if(!blockdev_is_ready()||base>blockdev_get_info()->sectors||sectors>blockdev_get_info()->sectors-base||sectors<16||!blockdev_read(base,1,io_sector)||!emrefs_validate_superblock(io_sector,sectors))return 0;
    uint64_t total=le64(io_sector+16),meta=le64(io_sector+32),data=le64(io_sector+40);uint32_t bits=bitmap_need(total-data);
    fs.mounted=1;fs.base=base;fs.total=total;fs.bitmap_lba=base+1;fs.bitmap_sectors=bits;fs.root_lba=fs.bitmap_lba+bits;fs.root_sectors=(uint32_t)meta-bits;fs.data_lba=base+data;fs.data_sectors=(uint32_t)(total-data);return 1;
}
int emrefs_auto_mount(void){
    uint8_t mbr[512];if(!blockdev_is_ready()||!blockdev_read(0,1,mbr)||mbr[510]!=0x55||mbr[511]!=0xAA)return 0;
    /* Custom EmreFS partition type 0xDA; the driver never guesses at arbitrary partitions. */
    for(unsigned i=0;i<4;++i){const uint8_t *e=&mbr[446+i*16];uint32_t start=le32(e+8),count=le32(e+12);if(e[4]==0xDA&&count&&start<blockdev_get_info()->sectors&&count<=blockdev_get_info()->sectors-start&&emrefs_mount(start,count))return 1;}return 0;
}
int emrefs_is_mounted(void){return fs.mounted;}
int emrefs_is_writable(void){return fs.mounted&&blockdev_is_writable();}
int emrefs_format(uint64_t base,uint64_t sectors){
    if(!blockdev_is_writable()||base>blockdev_get_info()->sectors||sectors>blockdev_get_info()->sectors-base||sectors<32||sectors>EMREFS_MAX_SECTORS)return 0;
    uint32_t data_guess=(uint32_t)(sectors-3),bits=bitmap_need(data_guess);if((uint64_t)bits+2>=sectors)return 0;uint32_t data_start=1+bits+1;uint32_t data_count=(uint32_t)(sectors-data_start);bits=bitmap_need(data_count);data_start=1+bits+1;data_count=(uint32_t)(sectors-data_start);
    if(!data_count||bits+1>=sectors)return 0;
    static const uint8_t zero[512]={0};for(uint32_t i=0;i<bits+1;++i)if(!blockdev_write(base+1+i,1,zero))return 0;
    clear_sector();static const uint8_t magic[8]={'E','M','R','E','F','S','1',0};for(unsigned i=0;i<8;++i)io_sector[i]=magic[i];
    put16(io_sector+8,1);put16(io_sector+10,64);put32(io_sector+12,512);put64(io_sector+16,sectors);put64(io_sector+24,1);put64(io_sector+32,(uint64_t)bits+1);put64(io_sector+40,data_start);put64(io_sector+48,1);put32(io_sector+60,crc32(io_sector,60));
    if(!blockdev_write(base,1,io_sector))return 0;return emrefs_mount(base,sectors);
}
static int valid_name(const char *n){if(!n||!*n)return 0;unsigned len=0;while(n[len]){if(len>=15||n[len]=='/'||n[len]=='\\'||n[len]<32)return 0;++len;}return 1;}
static int find_entry(const char *name,uint64_t *lba,unsigned *offset,uint8_t *entry){
    if(!fs.mounted||!valid_name(name))return 0;for(uint32_t s=0;s<fs.root_sectors;++s){if(!blockdev_read(fs.root_lba+s,1,io_sector))return 0;for(unsigned o=0;o<512;o+=EMREFS_ENTRY_SIZE){uint8_t *e=&io_sector[o];if(e[0]==0){if(lba)*lba=fs.root_lba+s;if(offset)*offset=o;if(entry)for(unsigned j=0;j<32;++j)entry[j]=e[j];return -1;}if(e[16]==0)continue;unsigned i=0;while(i<16&&e[i]&&name[i]&&e[i]==(uint8_t)name[i])++i;if(i==16||(!e[i]&&!name[i])){if(lba)*lba=fs.root_lba+s;if(offset)*offset=o;if(entry)for(unsigned j=0;j<32;++j)entry[j]=e[j];return 1;}}}return 0;
}
static int free_slot(uint64_t *lba,unsigned *off){for(uint32_t s=0;s<fs.root_sectors;++s){if(!blockdev_read(fs.root_lba+s,1,io_sector))return 0;for(unsigned o=0;o<512;o+=32)if(io_sector[o]==0||io_sector[o+16]==0){*lba=fs.root_lba+s;*off=o;return 1;}}return 0;}
static int bitmap_get(uint32_t idx,int *used){uint32_t sec=idx/4096u,bit=idx%4096u;if(sec>=fs.bitmap_sectors||!blockdev_read(fs.bitmap_lba+sec,1,io_sector))return 0;*used=(io_sector[bit/8]>>(bit%8))&1;return 1;}
static int bitmap_set(uint32_t idx,int used){uint32_t sec=idx/4096u,bit=idx%4096u;if(sec>=fs.bitmap_sectors||!blockdev_read(fs.bitmap_lba+sec,1,io_sector))return 0;uint8_t mask=(uint8_t)(1u<<(bit%8));if(used)io_sector[bit/8]|=mask;else io_sector[bit/8]&=(uint8_t)~mask;return blockdev_write(fs.bitmap_lba+sec,1,io_sector);}
static int allocate_run(uint32_t needed,uint32_t *first){if(!needed){*first=0;return 1;}uint32_t run=0,start=0;for(uint32_t i=0;i<fs.data_sectors;++i){int used;if(!bitmap_get(i,&used))return 0;if(used){run=0;continue;}if(!run)start=i;if(++run==needed){for(uint32_t j=0;j<needed;++j)if(!bitmap_set(start+j,1))return 0;*first=start;return 1;}}return 0;}
static void release_run(uint32_t first,uint32_t count){for(uint32_t i=0;i<count;++i)(void)bitmap_set(first+i,0);}
static int store_entry(const char *name,uint8_t type,uint32_t first,uint32_t size){uint64_t lba;unsigned off;uint8_t old[32];int exists=find_entry(name,&lba,&off,old);if(exists==0)return 0;if(exists==-1){/* find_entry returned the first free slot */}if(!blockdev_read(lba,1,io_sector))return 0;uint8_t *e=&io_sector[off];for(unsigned i=0;i<32;++i)e[i]=0;unsigned n=0;while(name[n]&&n<15){e[n]=(uint8_t)name[n];++n;}e[16]=type;put32(e+20,first);put32(e+24,size);return blockdev_write(lba,1,io_sector);}
int emrefs_mkdir(const char *name){if(!emrefs_is_writable()||!valid_name(name))return 0;uint64_t l;unsigned o;if(find_entry(name,&l,&o,0)==1)return 0;return store_entry(name,EMREFS_ENTRY_DIR,0,0);}
int emrefs_write_file(const char *name,const void *data,uint32_t size){
    if(!emrefs_is_writable()||!valid_name(name)||(!data&&size))return 0;uint64_t old_lba;unsigned old_off;uint8_t old[32];int exists=find_entry(name,&old_lba,&old_off,old);if(exists==0)return 0;if(exists==1&&old[16]!=EMREFS_ENTRY_FILE)return 0;
    uint32_t old_first=exists==1?le32(old+20):0,old_size=exists==1?le32(old+24):0,old_count=(old_size+511u)/512u;
    uint32_t count=(size+511u)/512u,first=0;if(!allocate_run(count,&first))return 0;
    const uint8_t *src=(const uint8_t*)data;for(uint32_t i=0;i<count;++i){clear_sector();uint32_t remain=size-i*512u,take=remain>512?512:remain;for(uint32_t j=0;j<take;++j)io_sector[j]=src[i*512u+j];if(!blockdev_write(fs.data_lba+first+i,1,io_sector)){release_run(first,count);return 0;}}
    uint64_t lba;unsigned off;if(exists==1){lba=old_lba;off=old_off;}else if(!free_slot(&lba,&off)){release_run(first,count);return 0;}
    if(!store_entry(name,EMREFS_ENTRY_FILE,first,size)){release_run(first,count);return 0;}
    if(exists==1&&old_count)release_run(old_first,old_count);return 1;
}
int emrefs_read_file(const char *name,void *out,uint32_t cap,uint32_t *bytes_read){if(bytes_read)*bytes_read=0;if(!fs.mounted||!out||!valid_name(name))return 0;uint8_t e[32];if(find_entry(name,0,0,e)!=1||e[16]!=EMREFS_ENTRY_FILE)return 0;uint32_t first=le32(e+20),size=le32(e+24);if(size>cap)return 0;uint8_t *dst=(uint8_t*)out;uint32_t count=(size+511u)/512u;for(uint32_t i=0;i<count;++i){if(first+i>=fs.data_sectors||!blockdev_read(fs.data_lba+first+i,1,io_sector))return 0;uint32_t take=size-i*512u;if(take>512)take=512;for(uint32_t j=0;j<take;++j)dst[i*512u+j]=io_sector[j];}if(bytes_read)*bytes_read=size;return 1;}
int emrefs_list_root(char *out,unsigned cap){if(!fs.mounted||!out||!cap)return -1;unsigned pos=0;int count=0;out[0]=0;for(uint32_t s=0;s<fs.root_sectors;++s){if(!blockdev_read(fs.root_lba+s,1,io_sector))return -1;for(unsigned o=0;o<512;o+=32){uint8_t *e=&io_sector[o];if(e[0]==0)return count;if(e[16]==0)continue;unsigned i=0;while(i<16&&e[i]&&pos+1<cap)out[pos++]=(char)e[i++];if(i==16&&e[i])return count;if(pos+1>=cap){out[pos]=0;return count;}const char *suffix=e[16]==EMREFS_ENTRY_DIR?"/\n":"\n";for(unsigned j=0;suffix[j]&&pos+1<cap;++j)out[pos++]=suffix[j];out[pos]=0;++count;}}return count;}
