/* SPDX-License-Identifier: MIT */
#include "uke/boot_core.h"

static uint16_t le16(const uint8_t *p) { return (uint16_t)((uint32_t)p[0] | (uint32_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int equal(const uint8_t *a, const uint8_t *b, size_t n) {
  size_t i; for (i=0;i<n;++i) if (a[i]!=b[i]) return 0; return 1;
}
static int string_equal(const char *a, const char *b) {
  size_t i=0;
  while(a[i] && b[i]) {
    char x=a[i],y=b[i];
    if(x>='a' && x<='z')x=(char)(x-'a'+'A');
    if(y>='a' && y<='z')y=(char)(y-'a'+'A');
    if(x!=y)return 0;
    ++i;
  }
  return a[i]==b[i];
}
static UKE_STATUS read_at(const UKE_FAT32 *v, uint64_t off, size_t n, void *out) {
  uint64_t length=(uint64_t)v->sectors*v->disk->block_size;
  if(off>length || n>length-off) return UKE_INVALID_FILESYSTEM;
  return v->disk->read(v->disk->context,v->offset+off,n,out) ? UKE_OK : UKE_IO_ERROR;
}
static int valid_cluster(const UKE_FAT32 *v, uint32_t c) { return c>=2 && c-2<v->clusters; }
static uint64_t cluster_offset(const UKE_FAT32 *v, uint32_t c) {
  return ((uint64_t)v->data_sector+(uint64_t)(c-2)*v->sectors_per_cluster)*v->disk->block_size;
}
static UKE_STATUS next_cluster(const UKE_FAT32 *v, uint32_t c, uint32_t *next) {
  uint8_t a[4],b[4];
  uint64_t off=((uint64_t)v->reserved_sectors+(uint64_t)v->active_fat*v->fat_sectors)*v->disk->block_size+(uint64_t)c*4;
  UKE_STATUS s=read_at(v,off,4,a);
  if(s!=UKE_OK)return s;
  *next=le32(a)&0x0fffffffu;
  if(v->mirrored && v->fat_count==2) {
    s=read_at(v,off+(uint64_t)v->fat_sectors*v->disk->block_size,4,b);
    if(s!=UKE_OK)return s;
    if((le32(b)&0x0fffffffu)!=*next)return UKE_INVALID_FILESYSTEM;
  }
  if(*next>=0x0ffffff8u)return UKE_OK;
  return valid_cluster(v,*next) ? UKE_OK : UKE_INVALID_FILESYSTEM;
}
UKE_STATUS uke_fat32_open(const UKE_DISK *d, const UKE_PARTITION *p, UKE_FAT32 *out) {
  uint8_t b[512],backup[90];
  UKE_FAT32 v;
  uint64_t part_bytes,data_sector,fat_entries;
  uint16_t flags,backup_sector;
  uint32_t total;
  if(!d || !p || !out || !d->read || d->block_size<512 || d->block_size>UKE_MAX_BLOCK ||
      (d->block_size&(d->block_size-1)) || d->bytes%d->block_size ||
      p->first_lba>p->last_lba || p->last_lba>=d->bytes/d->block_size) return UKE_INVALID_ARGUMENT;
  v.disk=d;v.offset=p->first_lba*d->block_size;
  part_bytes=(p->last_lba-p->first_lba+1)*d->block_size;
  if(!d->read(d->context,v.offset,sizeof(b),b))return UKE_IO_ERROR;
  if(b[510]!=0x55 || b[511]!=0xaa || le16(b+11)!=d->block_size || le16(b+17)!=0 ||
      le16(b+19)!=0 || le16(b+22)!=0 || le16(b+42)!=0) return UKE_INVALID_FILESYSTEM;
  v.sectors_per_cluster=b[13];v.reserved_sectors=le16(b+14);v.fat_count=b[16];
  total=le32(b+32);v.sectors=total;v.fat_sectors=le32(b+36);v.root_cluster=le32(b+44);
  flags=le16(b+40);v.mirrored=(flags&0x80u)==0;v.active_fat=v.mirrored ? 0 : (uint8_t)(flags&15u);
  if(!v.sectors_per_cluster || v.sectors_per_cluster>128 ||
      (v.sectors_per_cluster&(v.sectors_per_cluster-1)) || !v.reserved_sectors ||
      (v.fat_count!=1 && v.fat_count!=2) || v.active_fat>=v.fat_count || !total ||
      !v.fat_sectors || (uint64_t)total*d->block_size>part_bytes || (flags&0xff70u))
    return UKE_INVALID_FILESYSTEM;
  data_sector=(uint64_t)v.reserved_sectors+(uint64_t)v.fat_count*v.fat_sectors;
  if(data_sector>=total)return UKE_INVALID_FILESYSTEM;
  v.data_sector=(uint32_t)data_sector;v.clusters=(total-v.data_sector)/v.sectors_per_cluster;
  fat_entries=(uint64_t)v.fat_sectors*d->block_size/4;
  if(v.clusters<65525 || v.clusters>0x0fffffedu || fat_entries<(uint64_t)v.clusters+2 ||
      !valid_cluster(&v,v.root_cluster))return UKE_INVALID_FILESYSTEM;
  backup_sector=le16(b+50);
  if(backup_sector!=0 && backup_sector!=0xffffu) {
    UKE_STATUS s;
    if(backup_sector>=v.reserved_sectors)return UKE_INVALID_FILESYSTEM;
    s=read_at(&v,(uint64_t)backup_sector*d->block_size,sizeof(backup),backup);
    if(s!=UKE_OK)return s;
    if(!equal(b,backup,sizeof(backup)))return UKE_INVALID_FILESYSTEM;
  }
  *out=v;return UKE_OK;
}
typedef struct {uint32_t cluster,size;uint8_t attributes;char name[64];} FILE_ENTRY;
typedef struct {
  uint32_t cluster,steps,entries;
  size_t offset;
  uint16_t long_name[65];
  uint8_t ordinal,total,checksum;
} DIRECTORY;
static uint8_t name_checksum(const uint8_t *name) {
  uint8_t sum=0;
  for(unsigned i=0;i<11;++i)sum=(uint8_t)(((sum&1u)<<7)+(sum>>1)+name[i]);
  return sum;
}
static UKE_STATUS directory_next(const UKE_FAT32 *v, DIRECTORY *d, FILE_ENTRY *out) {
  static const uint8_t positions[13]={1,3,5,7,9,14,16,18,20,22,24,28,30};
  size_t cluster_bytes=(size_t)v->sectors_per_cluster*v->disk->block_size;
  while(valid_cluster(v,d->cluster)) {
    uint8_t e[32];
    UKE_STATUS s;
    size_t length=0;
    if(d->offset==cluster_bytes) {
      if(++d->steps>=128)return UKE_INVALID_FILESYSTEM;
      s=next_cluster(v,d->cluster,&d->cluster);if(s!=UKE_OK)return s;
      if(d->cluster>=0x0ffffff8u)return UKE_FILE_MISSING;
      d->offset=0;
    }
    if(++d->entries>4096)return UKE_INVALID_FILESYSTEM;
    s=read_at(v,cluster_offset(v,d->cluster)+d->offset,32,e);if(s!=UKE_OK)return s;
    d->offset+=32;
    if(!e[0])return UKE_FILE_MISSING;
    if(e[0]==0xe5) {d->ordinal=d->total=0;continue;}
    if(e[11]==0x0f) {
      uint8_t order=e[0]&31u;
      if((e[0]&0xa0u) || !order || order>20 || e[12] || le16(e+26))return UKE_INVALID_FILESYSTEM;
      if(e[0]&0x40u) {d->total=d->ordinal=order;d->checksum=e[13];}
      if(!d->total || order!=d->ordinal || e[13]!=d->checksum)return UKE_INVALID_FILESYSTEM;
      if(d->total<=5)for(unsigned i=0;i<13;++i)
        d->long_name[(order-1u)*13+i]=le16(e+positions[i]);
      --d->ordinal;continue;
    }
    out->name[0]=0;
    if(d->total) {
      if(d->ordinal || name_checksum(e)!=d->checksum)return UKE_INVALID_FILESYSTEM;
      if(d->total<=5) {
        size_t units=(size_t)d->total*13;
        while(length<units && d->long_name[length] && d->long_name[length]!=0xffffu)++length;
        if(length<units) {
          if(d->long_name[length]!=0)return UKE_INVALID_FILESYSTEM;
          for(size_t i=length+1;i<units;++i)if(d->long_name[i]!=0xffffu)return UKE_INVALID_FILESYSTEM;
        }
        if(length<sizeof(out->name)) {
          for(size_t i=0;i<length;++i) {
            uint16_t c=d->long_name[i];
            if(c<0x21 || c>0x7e) {length=0;break;}
            out->name[i]=(char)c;
          }
        } else length=0;
      }
    } else {
      size_t base=8,ext=3;
      while(base && e[base-1]==' ')--base;
      while(ext && e[8+ext-1]==' ')--ext;
      for(size_t i=0;i<base;++i)out->name[length++]=(char)e[i];
      if(ext) {out->name[length++]='.';for(size_t i=0;i<ext;++i)out->name[length++]=(char)e[8+i];}
    }
    d->ordinal=d->total=0;
    out->name[length]=0;
    if((e[11]&8) || !length || string_equal(out->name,".") || string_equal(out->name,".."))continue;
    out->cluster=((uint32_t)le16(e+20)<<16)|le16(e+26);
    out->size=le32(e+28);out->attributes=e[11];
    if((e[11]&0xc0) || ((!valid_cluster(v,out->cluster)) && ((e[11]&0x10) || out->size)))
      return UKE_INVALID_FILESYSTEM;
    return UKE_OK;
  }
  return UKE_INVALID_FILESYSTEM;
}
static UKE_STATUS lookup(const UKE_FAT32 *v, uint32_t dir, const char *name, FILE_ENTRY *out) {
  DIRECTORY d={0};FILE_ENTRY entry;UKE_STATUS s;unsigned found=0;
  d.cluster=dir;
  while((s=directory_next(v,&d,&entry))==UKE_OK)if(string_equal(entry.name,name)) {
    if(found++)return UKE_DIRECTORY_AMBIGUOUS;
    *out=entry;
  }
  return s==UKE_FILE_MISSING ? (found ? UKE_OK : UKE_FILE_MISSING) : s;
}
static UKE_STATUS read_file(const UKE_FAT32 *v, const FILE_ENTRY *entry, void *buffer, size_t cap, size_t *bytes) {
  uint32_t current=entry->cluster;
  size_t done=0,cluster_bytes=(size_t)v->sectors_per_cluster*v->disk->block_size;
  uint8_t *output=(uint8_t *)buffer;
  if(entry->attributes&0x10)return UKE_INVALID_FILESYSTEM;
  if(!entry->size || entry->size>cap || entry->size>UKE_MAX_IMAGE)return UKE_FILE_TOO_LARGE;
  while(done<entry->size) {
    size_t take=entry->size-done;uint32_t next;
    UKE_STATUS s;
    if(take>cluster_bytes)take=cluster_bytes;
    s=read_at(v,cluster_offset(v,current),take,output+done);if(s!=UKE_OK)return s;
    s=next_cluster(v,current,&next);if(s!=UKE_OK)return s;
    done+=take;
    if(done==entry->size) {if(next<0x0ffffff8u)return UKE_INVALID_FILESYSTEM;}
    else {if(!valid_cluster(v,next))return UKE_INVALID_FILESYSTEM;current=next;}
  }
  *bytes=done;return UKE_OK;
}
UKE_STATUS uke_fat32_read(const UKE_FAT32 *v, const char *path, void *buffer, size_t cap, size_t *bytes) {
  FILE_ENTRY entry;
  uint32_t dir;
  size_t start=0;
  UKE_STATUS s;
  if(bytes)*bytes=0;
  if(!v || !v->disk || !buffer || !bytes || !uke_esp_path_valid(path))return UKE_INVALID_ARGUMENT;
  dir=v->root_cluster;
  for(unsigned component=0;component<3;++component) {
    char name[64];size_t n=0;
    while(path[start] && path[start]!='/')name[n++]=path[start++];
    name[n]=0;
    s=lookup(v,dir,name,&entry);if(s!=UKE_OK)return s;
    if(component<2) {
      if(!(entry.attributes&0x10))return UKE_INVALID_FILESYSTEM;
      dir=entry.cluster;++start;
    }
  }
  return read_file(v,&entry,buffer,cap,bytes);
}
static int efi_file(const char *name) {
  size_t n=0;while(name[n])++n;
  return n>4 && string_equal(name+n-4,".EFI");
}
static int installed_path(char *out, const char *vendor, const char *file) {
  size_t n=0;
  const char *parts[3]={"EFI/",vendor,"/"};
  for(unsigned i=0;i<3;++i)for(size_t j=0;parts[i][j];++j) {
    if(n+1>=UKE_MANAGER_PATH_BYTES)return 0;
    out[n++]=parts[i][j];
  }
  for(size_t j=0;file[j];++j) {if(n+1>=UKE_MANAGER_PATH_BYTES)return 0;out[n++]=file[j];}
  out[n]=0;return uke_esp_path_valid(out);
}
UKE_STATUS uke_esp_manager_probe(const UKE_FAT32 *v, const char *profile,
  void *image, size_t cap, UKE_BOOT_CONFIG *out, size_t *bytes) {
  UKE_BOOT_CONFIG selected={0};
  FILE_ENTRY root,vendor,file;
  DIRECTORY vendors={0};
  UKE_STATUS s;
  size_t n=0,profile_size=0;
  unsigned directories=0,files=0,found=0;
  if(out)*out=(UKE_BOOT_CONFIG){0};
  if(bytes)*bytes=0;
  if(!v || !v->disk || !profile || !image || !out || !bytes)return UKE_INVALID_ARGUMENT;
  while(profile_size<UKE_PROFILE_BYTES && profile[profile_size])++profile_size;
  if(!profile_size || profile_size==UKE_PROFILE_BYTES)return UKE_INVALID_ARGUMENT;
  for(size_t i=0;i<profile_size;++i)selected.profile[i]=profile[i];
  selected.manager=UKE_MANAGER_DISCOVERED;
  s=uke_fat32_read(v,"EFI/BOOT/BOOTAA64.EFI",image,cap,&n);
  if(s==UKE_OK) {
    uke_sha256(image,n,selected.sha256);
    s=uke_image_check(image,n,selected.sha256);if(s!=UKE_OK)return s;
    (void)installed_path(selected.path,"BOOT","BOOTAA64.EFI");
    *out=selected;*bytes=n;return UKE_OK;
  }
  if(s!=UKE_FILE_MISSING)return s;
  s=lookup(v,v->root_cluster,"EFI",&root);if(s!=UKE_OK)return s;
  if(!(root.attributes&0x10))return UKE_INVALID_FILESYSTEM;
  vendors.cluster=root.cluster;
  /* shortcut: scan one EFI vendor level; nested/Unicode installs use BOOTAA64 until needed. */
  while((s=directory_next(v,&vendors,&vendor))==UKE_OK) {
    DIRECTORY entries={0};
    if(!(vendor.attributes&0x10) || string_equal(vendor.name,"UKE"))continue;
    if(++directories>32)return UKE_INVALID_FILESYSTEM;
    entries.cluster=vendor.cluster;
    while((s=directory_next(v,&entries,&file))==UKE_OK) {
      char path[UKE_MANAGER_PATH_BYTES]={0};uint8_t digest[32];
      if(++files>1024)return UKE_INVALID_FILESYSTEM;
      if((file.attributes&0x10) || !efi_file(file.name) || !installed_path(path,vendor.name,file.name))continue;
      s=read_file(v,&file,image,cap,&n);
      if(s==UKE_FILE_TOO_LARGE)continue;
      if(s!=UKE_OK)return s;
      uke_sha256(image,n,digest);
      if(uke_image_check(image,n,digest)!=UKE_OK)continue;
      if(found++)return UKE_AMBIGUOUS_MANAGER;
      for(size_t i=0;i<sizeof(selected.path);++i)selected.path[i]=path[i];
      for(size_t i=0;i<32;++i)selected.sha256[i]=digest[i];
    }
    if(s!=UKE_FILE_MISSING)return s;
  }
  if(s!=UKE_FILE_MISSING)return s;
  if(found) {
    s=uke_fat32_read(v,selected.path,image,cap,&n);if(s!=UKE_OK)return s;
    s=uke_image_check(image,n,selected.sha256);if(s!=UKE_OK)return s;
    *out=selected;*bytes=n;return UKE_OK;
  }
  {
    uint8_t text[UKE_MAX_CONFIG];
    s=uke_fat32_read(v,"EFI/UKE/BOOT.CFG",text,sizeof(text),&n);if(s!=UKE_OK)return s;
    s=uke_config_parse(text,n,profile,&selected);if(s!=UKE_OK)return s;
    s=uke_fat32_read(v,uke_manager_path(&selected),image,cap,&n);if(s!=UKE_OK)return s;
    s=uke_image_check(image,n,selected.sha256);if(s!=UKE_OK)return s;
    *out=selected;*bytes=n;return UKE_OK;
  }
}
