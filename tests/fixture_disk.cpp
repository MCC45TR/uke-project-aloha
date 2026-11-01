// SPDX-License-Identifier: MIT
// Creates disposable synthetic GPT containers only, with exclusive file creation.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

static void put32(uint8_t* p,uint32_t v) {for(unsigned i=0;i<4;++i)p[i]=static_cast<uint8_t>(v>>(i*8));}
static void put64(uint8_t* p,uint64_t v) {for(unsigned i=0;i<8;++i)p[i]=static_cast<uint8_t>(v>>(i*8));}
static uint32_t crc(const uint8_t* p,size_t n) {
  uint32_t c=0xffffffffu;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned j=0;j<8;++j)c=(c&1)?(c>>1)^0xedb88320u:c>>1;}return ~c;
}
static bool write_all(int fd,const uint8_t* p,size_t n,uint64_t offset) {
  while(n) {const auto r=pwrite(fd,p,n,static_cast<off_t>(offset));if(r<=0)return false;
    p+=r;n-=static_cast<size_t>(r);offset+=static_cast<uint64_t>(r);}return true;
}
int main(int argc,char** argv) {
  if(argc!=4)return 2;
  const std::string_view mode(argv[3]);
  if(mode!="one" && mode!="none" && mode!="two" && mode!="unnamed")return 2;
  const int source=open(argv[1],O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
  struct stat st{};
  if(source<0 || fstat(source,&st) || !S_ISREG(st.st_mode) || st.st_size<32*1024*1024 ||
      st.st_size>128*1024*1024 || st.st_size%512) {if(source>=0)close(source);return 2;}
  const auto volume=static_cast<uint64_t>(st.st_size)/512;
  const uint64_t count=mode=="two" ? 2 : 1, blocks=2048+volume*count+2048;
  const int output=open(argv[2],O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
  if(output<0) {close(source);return 2;}
  std::array<uint8_t,512> mbr{};std::array<uint8_t,16384> entries{};
  mbr[450]=0xee;put32(mbr.data()+454,1);put32(mbr.data()+458,static_cast<uint32_t>(blocks-1));mbr[510]=0x55;mbr[511]=0xaa;
  const uint8_t esp[16]={0x28,0x73,0x2a,0xc1,0x1f,0xf8,0xd2,0x11,0xba,0x4b,0,0xa0,0xc9,0x3e,0xc9,0x3b};
  for(uint64_t i=0;i<count;++i) {
    auto* e=entries.data()+i*128;if(mode!="none")std::memcpy(e,esp,16);else e[0]=1;
    e[16]=static_cast<uint8_t>(i+1);put64(e+32,2048+volume*i);put64(e+40,2048+volume*(i+1)-1);
    if(mode!="unnamed") {e[56]='E';e[58]='S';e[60]='P';}
  }
  bool ok=ftruncate(output,static_cast<off_t>(blocks*512))==0 && write_all(output,mbr.data(),mbr.size(),0);
  for(uint64_t lba:{uint64_t{1},blocks-1}) {
    std::array<uint8_t,512> header{};auto* h=header.data();std::memcpy(h,"EFI PART",8);
    put32(h+8,0x10000);put32(h+12,92);put64(h+24,lba);put64(h+32,lba==1 ? blocks-1 : 1);
    put64(h+40,34);put64(h+48,blocks-34);h[56]=42;
    const uint64_t array=lba==1 ? 2 : blocks-33;
    put64(h+72,array);put32(h+80,128);put32(h+84,128);put32(h+88,crc(entries.data(),entries.size()));put32(h+16,crc(h,92));
    ok=ok && write_all(output,h,header.size(),lba*512) && write_all(output,entries.data(),entries.size(),array*512);
  }
  std::array<uint8_t,65536> buffer{};
  for(uint64_t i=0;ok && i<count;++i) {
    for(uint64_t offset=0;ok && offset<volume*512;) {
      const auto n=pread(source,buffer.data(),buffer.size(),static_cast<off_t>(offset));
      if(n<=0){ok=false;break;}
      ok=write_all(output,buffer.data(),static_cast<size_t>(n),(2048+volume*i)*512+offset);
      offset+=static_cast<uint64_t>(n);
    }
  }
  close(source);if(close(output)!=0)ok=false;
  if(!ok)std::fputs("Synthetic fixture creation failed\n",stderr);
  return ok ? 0 : 1;
}
