#include "fat32.h"
#include "blockdev.h"
#include <stddef.h>

#define FAT32_EOC 0x0FFFFFF8u
#define FAT32_BAD 0x0FFFFFF7u
#define FAT32_MAX_CLUSTER_STEPS 16384u
struct fat_volume {
    int mounted;
    uint64_t base, sectors, fat_lba, data_lba;
    uint32_t fat_sectors, total_sectors, root_cluster, cluster_count;
    uint8_t sectors_per_cluster, fat_count;
};
static struct fat_volume vol;
static uint8_t sector[BLOCKDEV_SECTOR_SIZE];
static uint16_t rd16(const uint8_t*p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t rd32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int power2(unsigned n){return n && !(n&(n-1));}
static int read_sector(uint64_t lba,uint8_t *dst){return blockdev_read(lba,1,dst);}
static int cluster_lba(uint32_t cluster,uint64_t *lba){
    if(cluster<2 || cluster>=vol.cluster_count+2) return 0;
    uint64_t off=(uint64_t)(cluster-2)*vol.sectors_per_cluster;
    if(off>=vol.total_sectors || vol.data_lba>UINT64_MAX-off) return 0;
    *lba=vol.data_lba+off; return *lba < vol.base+vol.total_sectors;
}
static int next_cluster(uint32_t cluster,uint32_t *next){
    uint64_t byteoff=(uint64_t)cluster*4;
    uint64_t lba=vol.fat_lba+byteoff/BLOCKDEV_SECTOR_SIZE;
    if(lba>=vol.fat_lba+vol.fat_sectors || !read_sector(lba,sector)) return 0;
    uint32_t v=rd32(&sector[byteoff%BLOCKDEV_SECTOR_SIZE])&0x0FFFFFFFu;
    if(v>=FAT32_EOC || v==FAT32_BAD || v<2 || v>=vol.cluster_count+2) return 0;
    *next=v; return 1;
}
void fat32_init(void){vol.mounted=0;vol.base=0;vol.sectors=0;}
int fat32_mount(uint64_t base,uint64_t partition_sectors){
    struct fat_volume v={0};
    if(!blockdev_is_ready() || !partition_sectors || base>blockdev_get_info()->sectors || partition_sectors>blockdev_get_info()->sectors-base || !read_sector(base,sector)) return 0;
    if(sector[510]!=0x55 || sector[511]!=0xAA) return 0;
    uint16_t bps=rd16(&sector[11]); uint8_t spc=sector[13]; uint16_t reserved=rd16(&sector[14]); uint8_t fats=sector[16];
    uint16_t root_entries=rd16(&sector[17]); uint16_t fatsz16=rd16(&sector[22]);
    uint32_t total=rd32(&sector[32]); uint32_t fatsz=rd32(&sector[36]); uint16_t ext_flags=rd16(&sector[40]); uint16_t fs_version=rd16(&sector[42]); uint32_t root=rd32(&sector[44]);
    if(bps!=512 || !power2(spc) || spc>8 || !reserved || !fats || fats>2 || root_entries!=0 || fatsz16!=0 || !fatsz || !total || root<2 || fs_version!=0) return 0;
    if ((ext_flags & 0x0080u) && (ext_flags & 0x000Fu) != 0) return 0;
    if(total>partition_sectors) return 0;
    uint64_t data_start=(uint64_t)reserved+(uint64_t)fats*fatsz;
    if(data_start>=total) return 0;
    uint32_t clusters=(uint32_t)((total-data_start)/spc);
    if(clusters<65525 || root>=clusters+2) return 0;
    if((uint64_t)fatsz*512 < ((uint64_t)clusters+2)*4) return 0;
    v.mounted=1;v.base=base;v.sectors=total;v.fat_lba=base+reserved;v.fat_sectors=fatsz;
    v.data_lba=base+data_start;v.total_sectors=total;v.root_cluster=root;v.cluster_count=clusters;v.sectors_per_cluster=spc;v.fat_count=fats;
    vol=v; return 1;
}
int fat32_auto_mount(void){
    uint8_t mbr[512]; if(!blockdev_is_ready() || !read_sector(0,mbr)) return 0;
    if(mbr[510]!=0x55 || mbr[511]!=0xAA) return 0;
    if(fat32_mount(0,blockdev_get_info()->sectors)) return 1;
    for(unsigned i=0;i<4;++i){const uint8_t *p=&mbr[446+i*16];uint8_t type=p[4];uint32_t start=rd32(&p[8]),count=rd32(&p[12]);
        if((type==0x0B||type==0x0C) && count && start<blockdev_get_info()->sectors && count<=blockdev_get_info()->sectors-start)
            if(fat32_mount(start,count)) return 1;
    }
    return 0;
}
int fat32_is_mounted(void){return vol.mounted;}
uint64_t fat32_partition_lba(void){return vol.mounted?vol.base:0;}
static int entry_name(const uint8_t *e,char *out,unsigned cap){
    if (!cap) return 0; unsigned p=0;
    for(unsigned i=0;i<8 && e[i]!=' ';++i){if(p+1>=cap)return 0;out[p++]=(char)e[i];}
    unsigned ext=0;for(unsigned i=8;i<11 && e[i]!=' ';++i)++ext;
    if(ext){if(p+1>=cap)return 0;out[p++]='.';for(unsigned i=8;i<11 && e[i]!=' ';++i){if(p+1>=cap)return 0;out[p++]=(char)e[i];}}
    out[p]=0;return 1;
}
static int entry_live(const uint8_t *e){return e[0]!=0 && e[0]!=0xE5 && e[11]!=0x0F && !(e[11]&0x08);}
static int name_equal_ci(const char *a,const char *b){while(*a&&*b){char x=*a++,y=*b++;if(x>='a'&&x<='z')x=(char)(x-'a'+'A');if(y>='a'&&y<='z')y=(char)(y-'a'+'A');if(x!=y)return 0;}return *a==0&&*b==0;}
struct found_entry { uint32_t cluster, size; uint8_t attr; };
static int find_in_dir(uint32_t dir_cluster,const char *name,struct found_entry *found){
    uint32_t c=dir_cluster;unsigned steps=0;
    while(c>=2 && c<vol.cluster_count+2 && steps++<FAT32_MAX_CLUSTER_STEPS){uint64_t lba;if(!cluster_lba(c,&lba))return 0;
        for(unsigned s=0;s<vol.sectors_per_cluster;++s){if(!read_sector(lba+s,sector))return 0;
            for(unsigned o=0;o<512;o+=32){const uint8_t *e=&sector[o];if(e[0]==0)return 0;if(!entry_live(e))continue;char nm[16];if(!entry_name(e,nm,sizeof(nm)))continue;
                if(name_equal_ci(nm,name)){found->cluster=((uint32_t)rd16(&e[20])<<16)|rd16(&e[26]);found->size=rd32(&e[28]);found->attr=e[11];return 1;}
            }
        }
        uint32_t n;if(!next_cluster(c,&n))return 0;c=n;
    }
    return 0;
}
static int next_component(const char **path,char out[13]){
    const char *p=*path;while(*p=='/')++p;if(!*p){*path=p;return 0;}
    unsigned n=0;while(*p&&*p!='/'){if(n>=12)return -1;out[n++]=*p++;}out[n]=0;while(*p=='/')++p;*path=p;return 1;
}
static int resolve_path(const char *path,struct found_entry *entry,int want_dir){
    if(!vol.mounted||!path||!entry)return 0;while(*path=='/')++path;
    uint32_t dir=vol.root_cluster;char component[13];int rc;
    if(!*path){entry->cluster=dir;entry->size=0;entry->attr=0x10;return want_dir;}
    for(;;){rc=next_component(&path,component);if(rc<=0)return 0;struct found_entry e;if(!find_in_dir(dir,component,&e))return 0;
        if(*path){if(!(e.attr&0x10)||e.cluster<2)return 0;dir=e.cluster;continue;}
        if(want_dir ? !(e.attr&0x10) : ((e.attr&0x10)!=0))return 0;*entry=e;return 1;
    }
}
int fat32_list_path(const char *path,char *out,unsigned cap){
    if(!out||!cap||!vol.mounted)return -1;struct found_entry d;if(!resolve_path(path?path:"",&d,1))return -1;
    out[0]=0;unsigned pos=0,steps=0;uint32_t c=d.cluster;int count=0;
    while(c>=2&&c<vol.cluster_count+2&&steps++<FAT32_MAX_CLUSTER_STEPS){uint64_t lba;if(!cluster_lba(c,&lba))return -1;
        for(unsigned s=0;s<vol.sectors_per_cluster;++s){if(!read_sector(lba+s,sector))return -1;
            for(unsigned o=0;o<512;o+=32){const uint8_t *e=&sector[o];if(e[0]==0)return count;if(!entry_live(e))continue;char name[16];if(!entry_name(e,name,sizeof(name)))continue;
                unsigned i=0;while(name[i]&&pos+1<cap)out[pos++]=name[i++];out[pos]=0;if(name[i])return count;
                if(e[11]&0x10){static const char suffix[]="/";if(pos+1<cap)out[pos++]=suffix[0];else return count;out[pos]=0;}
                if(pos+1<cap){out[pos++]='\n';out[pos]=0;}++count;
            }
        }
        uint32_t n;if(!next_cluster(c,&n))break;c=n;
    }
    return count;
}
int fat32_list_root(char *out,unsigned cap){return fat32_list_path("/",out,cap);}
int fat32_read_file(const char *path,char *out,unsigned cap,unsigned *bytes_read){
    if(bytes_read)*bytes_read=0;
    if(!out||cap<1||!vol.mounted||!path)return -1;out[0]=0;struct found_entry e;if(!resolve_path(path,&e,0))return -1;
    if(!e.size){if(bytes_read)*bytes_read=0;return 0;}if(e.cluster<2||e.cluster>=vol.cluster_count+2)return -1;
    uint32_t c=e.cluster,copied=0;unsigned steps=0;
    while(copied<e.size&&copied+1<cap&&steps++<FAT32_MAX_CLUSTER_STEPS){uint64_t lba;if(!cluster_lba(c,&lba))return -1;
        for(unsigned s=0;s<vol.sectors_per_cluster&&copied<e.size&&copied+1<cap;++s){if(!read_sector(lba+s,sector))return -1;unsigned take=e.size-copied;if(take>512)take=512;if(take>cap-1-copied)take=cap-1-copied;for(unsigned j=0;j<take;++j)out[copied+j]=(char)sector[j];copied+=take;}
        if(copied>=e.size||copied+1>=cap)break;uint32_t n;if(!next_cluster(c,&n))return -1;c=n;
    }
    out[copied]=0;if(bytes_read)*bytes_read=copied;return copied==e.size?0:1;
}
int fat32_read_root_file(const char *name,char *out,unsigned cap,unsigned *bytes_read){return fat32_read_file(name,out,cap,bytes_read);}
