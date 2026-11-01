// SPDX-License-Identifier: MIT
#include "uke/boot_core.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

static unsigned assertions = 0;
static void check(bool ok, const char* why) {
  ++assertions;
  if (!ok) throw std::runtime_error(why);
}
static void put32(uint8_t* p, uint32_t v) {
  for (unsigned i=0;i<4;++i) p[i]=static_cast<uint8_t>(v>>(i*8));
}
static void put64(uint8_t* p, uint64_t v) {
  for (unsigned i=0;i<8;++i) p[i]=static_cast<uint8_t>(v>>(i*8));
}
// Independent byte/bit reference for constructing fixture checksums.
static uint32_t crc(const uint8_t* p, size_t n) {
  uint32_t c=0xffffffffu;
  for(size_t i=0;i<n;++i) {
    c^=p[i];
    for(unsigned bit=0;bit<8;++bit) c=(c&1) ? (c>>1)^0xedb88320u : c>>1;
  }
  return c^0xffffffffu;
}
static constexpr std::array<uint8_t,16> esp_type =
  {0x28,0x73,0x2a,0xc1,0x1f,0xf8,0xd2,0x11,0xba,0x4b,0x00,0xa0,0xc9,0x3e,0xc9,0x3b};
struct Fixture {
  uint32_t block;
  std::vector<uint8_t> data;
  unsigned reads=0;
  bool fail=false;
  explicit Fixture(uint32_t b=512) : block(b),data(static_cast<size_t>(b)*256) {
    data[510]=0x55; data[511]=0xaa;
    data[450]=0xee; put32(&data[454],1); put32(&data[458],255);
    for (uint64_t lba : {uint64_t{1},uint64_t{255}}) {
      auto* h=&data[static_cast<size_t>(lba)*block];
      std::memcpy(h,"EFI PART",8); put32(h+8,0x10000); put32(h+12,92);
      put64(h+24,lba); put64(h+32,lba==1 ? 255 : 1);
      put64(h+40,34); put64(h+48,222); h[56]=9;
      put64(h+72,lba==1 ? 2 : 223); put32(h+80,128); put32(h+84,128);
    }
    partition(0,40,49,true); refresh();
  }
  void partition(unsigned index, uint64_t first, uint64_t last, bool is_esp) {
    auto* p=&data[static_cast<size_t>(block)*2+index*128];
    std::fill(p,p+128,0);
    if(is_esp) std::copy(esp_type.begin(),esp_type.end(),p); else p[0]=1;
    p[16]=static_cast<uint8_t>(index+1); put64(p+32,first); put64(p+40,last);
    if(is_esp) {p[56]='E';p[58]='S';p[60]='P';}
  }
  void refresh() {
    auto* primary=&data[static_cast<size_t>(block)*2];
    auto* backup=&data[static_cast<size_t>(block)*223];
    std::copy(primary,primary+128*128,backup);
    for(size_t lba : {size_t{1},size_t{255}}) {
      auto* h=&data[lba*block]; put32(h+88,crc(primary,128*128));
      put32(h+16,0); put32(h+16,crc(h,92));
    }
  }
  static int read(void* ptr,uint64_t offset,size_t bytes,void* output) {
    auto& f=*static_cast<Fixture*>(ptr); ++f.reads;
    check(offset<=f.data.size() && bytes<=f.data.size()-offset,"read escaped fixture");
    if(f.fail) return 0;
    std::memcpy(output,f.data.data()+static_cast<size_t>(offset),bytes); return 1;
  }
  UKE_STATUS scan(UKE_GPT_RESULT& result) {
    UKE_DISK disk{this,read,data.size(),block}; UKE_GPT_WORKSPACE w{};
    reads=0;
    const auto status=uke_gpt_scan(&disk,&w,&result);
    check(reads<=260,"unbounded scanner I/O"); return status;
  }
};
static void gpt_tests() {
  check(uke_crc32("123456789",9)==0xcbf43926u,"CRC standard vector");
  for(uint32_t b : {512u,4096u}) {
    Fixture f(b); UKE_GPT_RESULT r{};
    check(f.scan(r)==UKE_OK && r.count==1 && r.esps[0].first_lba==40 && r.named_esp_mask==1,"valid named ESP GPT");
    f.data[2*b+56]='e';f.data[2*b+58]='s';f.data[2*b+60]='p';f.refresh();
    check(f.scan(r)==UKE_OK && r.named_esp_mask==1,"case insensitive ESP GPT name");
    f.data[2*b+62]='X';f.refresh();
    check(f.scan(r)==UKE_OK && r.named_esp_mask==0,"ESP suffix does not match");
    f.partition(0,40,49,false); f.refresh();
    check(f.scan(r)==UKE_OK && r.count==0,"ordinary GPT has no ESP");
  }
  auto expect=[](auto mutate,UKE_STATUS expected,const char* why) {
    Fixture f; UKE_GPT_RESULT r{}; mutate(f); check(f.scan(r)==expected,why);
    check(r.count==0 && r.disk_guid[0]==0 && r.named_esp_mask==0,"failed scan exposes no candidates");
  };
  expect([](Fixture& f){f.data[510]=0;},UKE_INVALID_MBR,"MBR signature");
  expect([](Fixture& f){f.data[466]=0x83;},UKE_INVALID_MBR,"hybrid MBR refused");
  expect([](Fixture& f){f.data[512+16]^=1;},UKE_INVALID_GPT,"header CRC");
  expect([](Fixture& f){f.data[255*512+16]^=1;},UKE_INVALID_GPT,"backup CRC");
  expect([](Fixture& f){f.data[223*512]^=1;},UKE_GPT_MISMATCH,"array divergence");
  expect([](Fixture& f){f.partition(1,45,55,false);f.refresh();},UKE_PARTITION_OVERLAP,"overlap");
  expect([](Fixture& f){f.partition(1,50,59,true);f.data[2*512+128+16]=1;f.refresh();},UKE_INVALID_GPT,"duplicate GUID");
  expect([](Fixture& f){f.partition(1,221,223,true);f.refresh();},UKE_INVALID_GPT,"partition past usable range");
  expect([](Fixture& f){f.partition(1,60,50,true);f.refresh();},UKE_INVALID_GPT,"reversed range");
  expect([](Fixture& f){for(unsigned i=0;i<17;++i)f.partition(i,40+i*4,42+i*4,true);f.refresh();},UKE_TOO_MANY_ESPS,"ESP count bound");
  expect([](Fixture& f){f.fail=true;},UKE_IO_ERROR,"I/O failure");
  expect([](Fixture& f){f.block=1023;},UKE_UNSUPPORTED_GEOMETRY,"sector geometry");
  expect([](Fixture& f){put32(&f.data[512+80],129);put32(&f.data[512+16],0);put32(&f.data[512+16],crc(&f.data[512],92));},UKE_UNSUPPORTED_GEOMETRY,"array count bound");
  Fixture original;
  uint32_t seed=0x12345678;
  for(unsigned i=0;i<1000;++i) {
    Fixture f=original; UKE_GPT_RESULT r{};
    seed=seed*1664525u+1013904223u;
    f.data[seed%f.data.size()]^=static_cast<uint8_t>(1u<<(seed%8));
    const auto s=f.scan(r);
    check(s>=UKE_OK && s<=UKE_DIRECTORY_AMBIGUOUS,"mutation status bound");
    if(s!=UKE_OK)check(r.count==0,"mutated GPT cannot expose partial candidates");
  }
}
static std::string config_text(std::string_view manager="refind",std::string_view digest=std::string(64,'0')) {
  return "schema=1\nprofile=uke-fixture\nmanager="+std::string(manager)+"\nsha256="+std::string(digest)+"\n";
}
static void config_tests() {
  auto parse=[](const std::string& s,const char* profile="uke-fixture") {
    UKE_BOOT_CONFIG c{};return uke_config_parse(s.data(),s.size(),profile,&c);
  };
  check(parse(config_text())==UKE_OK,"rEFInd config");
  check(parse(config_text("systemd-boot"))==UKE_OK,"systemd config");
  check(parse(config_text(),"other")==UKE_WRONG_PROFILE,"exact profile");
  check(parse(config_text()+"schema=1\n")==UKE_INVALID_CONFIG,"duplicate schema");
  check(parse(config_text()+"path=../../EFI/BOOT/BOOTAA64.EFI\n")==UKE_INVALID_CONFIG,"path injection");
  check(parse(config_text("windows"))==UKE_INVALID_CONFIG,"Windows disabled");
  check(parse(config_text("refind",std::string(64,'G')))==UKE_INVALID_CONFIG,"invalid hash");
  check(parse("schema=2\n")==UKE_INVALID_CONFIG,"unknown schema");
  check(parse(std::string(513,'a'))==UKE_INVALID_CONFIG,"config size bound");
  auto nul=config_text();nul[10]=0;
  check(parse(nul)==UKE_INVALID_CONFIG,"embedded null");
  auto crlf=config_text();crlf.insert(8,"\r");
  check(parse(crlf)==UKE_INVALID_CONFIG,"strict line format");
  auto no_newline=config_text();no_newline.pop_back();
  check(parse(no_newline)==UKE_OK,"final line without LF");
  UKE_BOOT_CONFIG empty{};
  check(uke_manager_path(&empty)==nullptr,"no unknown path");
}
struct Verify {
  std::array<UKE_STATUS,16> statuses{};
  bool invalid_manager=false;
  unsigned calls=0;
  static UKE_STATUS call(void* ptr,size_t,const UKE_PARTITION* p,UKE_BOOT_CONFIG* c) {
    auto& v=*static_cast<Verify*>(ptr);++v.calls;
    const auto s=v.statuses[p->guid[0]%16];
    c->manager=v.invalid_manager && p->guid[0]==2 ? UKE_MANAGER_NONE : UKE_MANAGER_REFIND;
    return s;
  }
};
static void policy_tests() {
  UKE_GPT_RESULT g{};Verify v;UKE_MEDIA media{1,&g,UKE_OK};
  UKE_DISCOVERY d{&media,1,nullptr,&v,Verify::call};
  auto a=uke_boot_decide(&d);
  check(a.action==UKE_BOOT_ANDROID && a.reason==UKE_NO_ESP,"no ESP Android");
  g.count=1;g.esps[0].guid[0]=1;g.named_esp_mask=1;
  a=uke_boot_decide(&d);
  check(a.action==UKE_BOOT_MANAGER,"single eligible ESP");
  g.named_esp_mask=0;v.calls=0;
  check(uke_boot_decide(&d).reason==UKE_NO_ESP && v.calls==0,"unnamed ESP not booted");
  g.named_esp_mask=1;
  uke_manager_return(&a);
  check(a.action==UKE_BOOT_ANDROID && a.reason==UKE_MANAGER_RETURNED,"manager returns");
  g.count=2;g.esps[1].guid[0]=2;g.named_esp_mask=3;
  check(uke_boot_decide(&d).reason==UKE_AMBIGUOUS_ESP,"multiple eligible ESPs");
  v.invalid_manager=true;a=uke_boot_decide(&d);
  check(a.action==UKE_BOOT_MANAGER && a.reason==UKE_OK,"ineligible later manager preserves selected result");
  v.invalid_manager=false;
  d.bound_partition_guid=g.esps[1].guid;
  a=uke_boot_decide(&d);
  check(a.action==UKE_BOOT_MANAGER && a.partition.guid[0]==2,"GUID binding");
  std::array<uint8_t,16> absent{};absent[0]=3;d.bound_partition_guid=absent.data();
  check(uke_boot_decide(&d).reason==UKE_BOUND_ESP_MISSING,"binding cannot fall through");
  d.bound_partition_guid=g.esps[0].guid;g.esps[1].guid[0]=1;
  check(uke_boot_decide(&d).reason==UKE_AMBIGUOUS_ESP,"cross-media duplicate binding");
  g.count=1;d.bound_partition_guid=nullptr;
  for(auto s : {UKE_INVALID_CONFIG,UKE_WRONG_PROFILE,UKE_INVALID_IMAGE,UKE_HASH_MISMATCH,UKE_IO_ERROR,UKE_BACKEND_UNAVAILABLE}) {
    v.statuses[1]=s;a=uke_boot_decide(&d);
    check(a.action==UKE_BOOT_ANDROID && a.reason==s,"verification failure Android");
  }
  v.statuses[1]=UKE_OK;media.admitted_internal=0;v.calls=0;
  check(uke_boot_decide(&d).action==UKE_BOOT_ANDROID && v.calls==0,"external device never queried");
  media.admitted_internal=1;media.scan_status=UKE_INVALID_GPT;
  check(uke_boot_decide(&d).reason==UKE_INVALID_GPT,"invalid media not ignored");
  media.scan_status=UKE_OK;g.count=17;
  check(uke_boot_decide(&d).action==UKE_BOOT_ANDROID,"untrusted count bounded");
  check(uke_boot_decide(nullptr).action==UKE_BOOT_ANDROID,"default-safe missing backend");
}
static std::string digest_hex(const uint8_t* data,size_t n) {
  uint8_t hash[32];uke_sha256(data,n,hash);std::string s;
  const char* hex="0123456789abcdef";
  for(auto b:hash) {s+=hex[b>>4];s+=hex[b&15];}return s;
}
static std::vector<uint8_t> image_fixture() {
  std::vector<uint8_t> p(1024);p[0]='M';p[1]='Z';put32(&p[60],128);
  std::memcpy(&p[128],"PE\0\0",4);p[132]=0x64;p[133]=0xaa;p[134]=1;p[148]=240;p[150]=2;
  p[152]=0x0b;p[153]=2;put32(&p[168],4096);put32(&p[184],4096);put32(&p[188],512);
  put32(&p[208],8192);put32(&p[212],512);p[220]=10;
  const size_t table=392;put32(&p[table+8],512);put32(&p[table+12],4096);
  put32(&p[table+16],512);put32(&p[table+20],512);put32(&p[table+36],0x60000020);
  return p;
}
static void image_tests() {
  check(digest_hex(nullptr,0)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA empty");
  const std::string abc="abc";
  check(digest_hex(reinterpret_cast<const uint8_t*>(abc.data()),abc.size())==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA abc");
  const std::string boundary="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  check(digest_hex(reinterpret_cast<const uint8_t*>(boundary.data()),boundary.size())==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","SHA two-block padding");
  const std::string million(1000000,'a');
  check(digest_hex(reinterpret_cast<const uint8_t*>(million.data()),million.size())==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","SHA million");
  auto p=image_fixture();uint8_t hash[32];uke_sha256(p.data(),p.size(),hash);
  check(uke_image_check(p.data(),p.size(),hash)==UKE_OK,"AArch64 EFI image");
  p.back()^=1;check(uke_image_check(p.data(),p.size(),hash)==UKE_HASH_MISMATCH,"payload mutation");p.back()^=1;
  p[132]=0x64;p[133]=0x86;check(uke_image_check(p.data(),p.size(),hash)==UKE_INVALID_IMAGE,"x86 EFI rejected");
  p=image_fixture();p[220]=11;check(uke_image_check(p.data(),p.size(),hash)==UKE_INVALID_IMAGE,"EFI driver rejected");
  p=image_fixture();put32(&p[60],0xffffffffu);check(uke_image_check(p.data(),p.size(),hash)==UKE_INVALID_IMAGE,"PE offset overflow");
  p=image_fixture();put32(&p[392+20],0xfffffff0u);check(uke_image_check(p.data(),p.size(),hash)==UKE_INVALID_IMAGE,"raw bounds");
  p=image_fixture();put32(&p[392+36],0x40000020);check(uke_image_check(p.data(),p.size(),hash)==UKE_INVALID_IMAGE,"non-executable entry");
  for(size_t n=0;n<1024;++n) {p=image_fixture();check(uke_image_check(p.data(),n,hash)!=UKE_OK,"truncated image refused");}
}
struct FatFixture {
  static constexpr uint32_t sectors=70000, fat=550, start=2048, data_sector=32+2*fat;
  std::vector<uint8_t> bytes=std::vector<uint8_t>(static_cast<size_t>(sectors)*512);
  bool fail=false;
  FatFixture() {
    bytes[0]=0xeb;bytes[1]=0x58;bytes[2]=0x90;
    bytes[11]=0;bytes[12]=2;bytes[13]=1;bytes[14]=32;bytes[16]=2;
    put32(&bytes[32],sectors);put32(&bytes[36],fat);put32(&bytes[44],2);bytes[50]=6;
    bytes[510]=0x55;bytes[511]=0xaa;
    std::copy_n(bytes.begin(),512,bytes.begin()+6*512);
    for(unsigned cluster=0;cluster<8;++cluster)set_fat(cluster,0x0fffffffu);
    directory(2,0,"EFI        ",3,0,true);
    directory(3,0,"UKE        ",4,0,true);
    const auto p=image_fixture();
    const auto cfg=config_text("refind",digest_hex(p.data(),p.size()));
    directory(4,0,"BOOT    CFG",5,static_cast<uint32_t>(cfg.size()),false);
    directory(4,1,"REFIND  EFI",6,static_cast<uint32_t>(p.size()),false);
    std::copy(cfg.begin(),cfg.end(),bytes.begin()+static_cast<ptrdiff_t>(offset(5)));
    std::copy(p.begin(),p.end(),bytes.begin()+static_cast<ptrdiff_t>(offset(6)));
    set_fat(6,7);
  }
  static size_t offset(uint32_t cluster) {return static_cast<size_t>(data_sector+cluster-2)*512;}
  void set_fat(unsigned cluster,uint32_t value) {
    put32(&bytes[32*512+cluster*4],value);put32(&bytes[(32+fat)*512+cluster*4],value);
  }
  void directory(uint32_t cluster,unsigned index,const char* name,uint32_t target,uint32_t size,bool dir) {
    auto* e=&bytes[offset(cluster)+index*32];std::memcpy(e,name,11);e[11]=dir ? 0x10 : 0x20;
    e[20]=static_cast<uint8_t>(target>>16);e[21]=static_cast<uint8_t>(target>>24);
    e[26]=static_cast<uint8_t>(target);e[27]=static_cast<uint8_t>(target>>8);put32(e+28,size);
  }
  void long_directory(uint32_t cluster,unsigned index,std::string_view name,const char* alias,uint32_t target,uint32_t size,bool dir=false) {
    const unsigned count=static_cast<unsigned>((name.size()+12)/13);
    unsigned checksum=0;
    for(unsigned i=0;i<11;++i)checksum=(((checksum&1u)<<7)+(checksum>>1)+static_cast<unsigned char>(alias[i]))&255u;
    const unsigned fields[13]={1,3,5,7,9,14,16,18,20,22,24,28,30};
    for(unsigned i=0;i<count;++i) {
      auto* e=&bytes[offset(cluster)+(index+i)*32];const unsigned order=count-i;
      e[0]=static_cast<uint8_t>(order|(i==0 ? 0x40u : 0));e[11]=0x0f;e[13]=static_cast<uint8_t>(checksum);
      for(unsigned j=0;j<13;++j) {
        const size_t position=(order-1u)*13+j;
        const unsigned value=position<name.size() ? static_cast<unsigned char>(name[position]) : (position==name.size() ? 0 : 0xffffu);
        e[fields[j]]=static_cast<uint8_t>(value);e[fields[j]+1]=static_cast<uint8_t>(value>>8);
      }
    }
    directory(cluster,index+count,alias,target,size,dir);
  }
  static int read(void* p,uint64_t off,size_t n,void* out) {
    auto& f=*static_cast<FatFixture*>(p);
    const uint64_t beginning=static_cast<uint64_t>(start)*512;
    check(off>=beginning && off-beginning<=f.bytes.size() && n<=f.bytes.size()-(off-beginning),"FAT escaped partition");
    if(f.fail)return 0;
    std::memcpy(out,f.bytes.data()+static_cast<size_t>(off-beginning),n);return 1;
  }
};
static void fat_tests() {
  FatFixture f;UKE_DISK d{&f,FatFixture::read,(FatFixture::start+FatFixture::sectors+100)*512ull,512};
  UKE_PARTITION p{{1},FatFixture::start,FatFixture::start+FatFixture::sectors-1};UKE_FAT32 v{};
  check(uke_fat32_open(&d,&p,&v)==UKE_OK,"FAT32 mount");
  std::array<uint8_t,2048> out{};size_t n=0;
  check(uke_fat32_read(&v,"EFI/UKE/BOOT.CFG",out.data(),512,&n)==UKE_OK,"FAT config");
  UKE_BOOT_CONFIG c{};check(uke_config_parse(out.data(),n,"uke-fixture",&c)==UKE_OK,"FAT config parse");
  check(uke_fat32_read(&v,uke_manager_path(&c),out.data(),out.size(),&n)==UKE_OK,"multi-cluster loader read");
  check(uke_image_check(out.data(),n,c.sha256)==UKE_OK,"FAT image preflight");
  check(uke_fat32_read(&v,"EFI/UKE/SDBOOT.EFI",out.data(),out.size(),&n)==UKE_FILE_MISSING,"absent manager");
  check(uke_fat32_read(&v,"EFI/../UKE/BOOT.CFG",out.data(),out.size(),&n)==UKE_INVALID_ARGUMENT,"path traversal");
  check(uke_fat32_read(&v,"EFI/UKE/REFIND.EFI",out.data(),10,&n)==UKE_FILE_TOO_LARGE,"caller buffer bound");
  f.set_fat(6,6);
  check(uke_fat32_read(&v,"EFI/UKE/REFIND.EFI",out.data(),out.size(),&n)==UKE_INVALID_FILESYSTEM,"file cycle");
  f.set_fat(6,7);put32(&f.bytes[(32+FatFixture::fat)*512+6*4],8);
  check(uke_fat32_read(&v,"EFI/UKE/REFIND.EFI",out.data(),out.size(),&n)==UKE_INVALID_FILESYSTEM,"FAT mirror mismatch");
  f.set_fat(6,7);f.directory(4,2,"BOOT    CFG",5,100,false);
  check(uke_fat32_read(&v,"EFI/UKE/BOOT.CFG",out.data(),out.size(),&n)==UKE_DIRECTORY_AMBIGUOUS,"duplicate directory entry");
  f.bytes[FatFixture::offset(4)+2*32]=0;
  f.bytes[6*512+13]=2;check(uke_fat32_open(&d,&p,&v)==UKE_INVALID_FILESYSTEM,"backup BPB mismatch");
  f.bytes[6*512+13]=1;f.fail=true;check(uke_fat32_open(&d,&p,&v)==UKE_IO_ERROR,"filesystem I/O error");
}
static void manager_tests() {
  FatFixture f;UKE_DISK d{&f,FatFixture::read,(FatFixture::start+FatFixture::sectors+100)*512ull,512};
  UKE_PARTITION p{{1},FatFixture::start,FatFixture::start+FatFixture::sectors-1};UKE_FAT32 v{};
  std::array<uint8_t,2048> image{};size_t n=0;UKE_BOOT_CONFIG config{};
  const auto original=image_fixture();
  check(uke_fat32_open(&d,&p,&v)==UKE_OK,"manager FAT32 opened");
  auto probe=[&] { return uke_esp_manager_probe(&v,"uke-fixture",image.data(),image.size(),&config,&n); };
  check(probe()==UKE_OK && config.manager==UKE_MANAGER_REFIND,"legacy config retained");
  f.directory(3,1,"REFIND     ",8,0,true);f.set_fat(8,0x0fffffffu);
  f.long_directory(8,0,"refind_aa64.efi","REFIND~1EFI",6,1024);
  f.bytes[FatFixture::offset(4)]=0; // No BOOT.CFG or project-owned copy.
  check(probe()==UKE_OK && config.manager==UKE_MANAGER_DISCOVERED &&
    std::string(uke_manager_path(&config))=="EFI/REFIND/refind_aa64.efi" && n==1024,"installed rEFInd without config");
  for(const auto field: {size_t{0},size_t{12},size_t{26},size_t{7},size_t{32}}) {
    auto& value=f.bytes[FatFixture::offset(8)+field];const auto saved=value;
    value^=0x20;
    check(probe()==UKE_INVALID_FILESYSTEM && config.manager==UKE_MANAGER_NONE && n==0,"malformed LFN metadata rejected");
    value=saved;
  }
  f.bytes[FatFixture::offset(8)+13]^=1;
  check(probe()==UKE_INVALID_FILESYSTEM && config.manager==UKE_MANAGER_NONE && n==0,"LFN checksum rejected");
  f.bytes[FatFixture::offset(8)+13]^=1;
  f.directory(3,2,"SYSTEMD    ",9,0,true);f.set_fat(9,0x0fffffffu);
  f.set_fat(11,12);f.set_fat(12,0x0fffffffu);
  auto wrong=image_fixture();wrong[132]=0x64;wrong[133]=0x86;
  std::copy(wrong.begin(),wrong.end(),f.bytes.begin()+static_cast<ptrdiff_t>(FatFixture::offset(11)));
  f.long_directory(9,0,"systemd-bootaa64.efi","SYSTEM~1EFI",11,1024);
  check(probe()==UKE_OK && std::equal(original.begin(),original.end(),image.begin()),"invalid sibling cannot clobber selected image");
  f.long_directory(9,0,"systemd-bootaa64.efi","SYSTEM~1EFI",6,1024);
  check(probe()==UKE_AMBIGUOUS_MANAGER && config.manager==UKE_MANAGER_NONE && n==0,"two installed managers ambiguous");
  f.directory(3,3,"BOOT       ",10,0,true);f.set_fat(10,0x0fffffffu);
  f.directory(10,0,"BOOTAA64EFI",6,1024,false);
  check(probe()==UKE_OK && std::string(uke_manager_path(&config))=="EFI/BOOT/BOOTAA64.EFI","default manager wins ambiguity");
  f.bytes[FatFixture::offset(6)+132]=0x64;f.bytes[FatFixture::offset(6)+133]=0x86;
  check(probe()==UKE_INVALID_IMAGE && config.manager==UKE_MANAGER_NONE && n==0,"invalid default never bypassed");
  std::copy(original.begin(),original.end(),f.bytes.begin()+static_cast<ptrdiff_t>(FatFixture::offset(6)));
  f.bytes[FatFixture::offset(10)]=0;
  f.bytes[FatFixture::offset(3)+32]=0xe5;
  check(probe()==UKE_OK && std::string(uke_manager_path(&config))=="EFI/SYSTEMD/systemd-bootaa64.efi","installed systemd without config");
  f.long_directory(9,0,"other_boot.efi","OTHERB~1EFI",6,1024);
  check(probe()==UKE_OK && std::string(uke_manager_path(&config))=="EFI/SYSTEMD/other_boot.efi","manager name not hardcoded");
  check(!uke_esp_path_valid("EFI/../BOOTAA64.EFI") && !uke_esp_path_valid("EFI/VENDOR/../../escape") &&
    !uke_esp_path_valid("EFI/VENDOR/file\\name.efi"),"discovered paths cannot escape ESP");
  f.fail=true;check(probe()==UKE_IO_ERROR && config.manager==UKE_MANAGER_NONE,"manager I/O failure refused");
}
static void button_tests() {
  UKE_BUTTONS b{};UKE_KEY_EVENT events[UKE_KEY_COUNT];
  auto poll=[&](uint64_t t,uint8_t mask){uke_buttons_poll(&b,t,mask,events);};
  uke_buttons_init(&b,0,4);poll(100,4);poll(200,0);poll(230,0);
  check(events[2]==UKE_EVENT_NONE,"initial held power release suppressed");
  poll(300,4);poll(310,0);poll(340,0);check(events[2]==UKE_EVENT_NONE,"power bounce suppressed");
  poll(400,4);poll(430,4);check(events[2]==UKE_EVENT_NONE,"power press does not select");
  poll(500,0);poll(530,0);check(events[2]==UKE_EVENT_SELECT,"short power release selects");
  poll(540,0);check(events[2]==UKE_EVENT_NONE,"no duplicate select");
  poll(600,4);poll(630,4);poll(1600,0);poll(1630,0);check(events[2]==UKE_EVENT_NONE,"long power escape");
  poll(1700,1);poll(1730,1);check(events[0]==UKE_EVENT_PREVIOUS,"volume initial event");
  poll(1800,1);check(events[0]==UKE_EVENT_NONE,"repeat delay");
  poll(2230,1);check(events[0]==UKE_EVENT_PREVIOUS,"volume delayed repeat");
  poll(10000,1);check(events[0]==UKE_EVENT_PREVIOUS,"one repeat after large time step");
  poll(10001,1);check(events[0]==UKE_EVENT_NONE,"no catch-up burst");
  poll(10100,5);poll(10130,5);poll(10200,1);poll(10230,1);
  check(events[2]==UKE_EVENT_NONE,"chord release cannot select");
  poll(50,4);poll(100,0);poll(130,0);check(events[2]==UKE_EVENT_NONE,"backwards clock suppression");
  uke_buttons_init(&b,0,0);
  for(uint64_t i=0;i<100;++i) {
    const uint64_t t=i*300+100;poll(t,4);poll(t+30,4);poll(t+100,0);poll(t+130,0);
    check(events[2]==UKE_EVENT_SELECT,"100 press/release fixture");
  }
}
int main(int argc,char** argv) {
  try {
    const std::string_view suite=argc==2 ? argv[1] : "";
    if(suite=="gpt")gpt_tests();else if(suite=="policy")policy_tests();else if(suite=="config")config_tests();
    else if(suite=="image")image_tests();else if(suite=="buttons")button_tests();else if(suite=="fat32")fat_tests();else if(suite=="manager")manager_tests();else return 2;
    std::cout<<suite<<": "<<assertions<<" assertions passed\n";
  } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
