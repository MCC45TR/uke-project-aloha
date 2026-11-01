// SPDX-License-Identifier: MIT
// Freestanding Linux ABI fixture for QEMU user mode; never a tablet payload.
#include "uke/boot_core.h"

static long syscall3(long number,long a,long b,long c) {
  register long x0 asm("x0")=a;
  register long x1 asm("x1")=b;
  register long x2 asm("x2")=c;
  register long x8 asm("x8")=number;
  asm volatile("svc #0" : "+r"(x0) : "r"(x1),"r"(x2),"r"(x8) : "memory");
  return x0;
}
static UKE_STATUS verify(void*,size_t,const UKE_PARTITION*,UKE_BOOT_CONFIG* out) {
  out->manager=UKE_MANAGER_REFIND;return UKE_OK;
}
extern "C" [[noreturn]] void _start() {
  unsigned errors=0;
  const char abc[]="abc";
  const uint8_t expected[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
                              0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
  uint8_t hash[32];uke_sha256(abc,3,hash);
  for(unsigned i=0;i<32;++i)if(hash[i]!=expected[i])++errors;
  if(uke_crc32("123456789",9)!=0xcbf43926u)++errors;
  UKE_GPT_RESULT g{};UKE_MEDIA m{1,&g,UKE_OK};UKE_DISCOVERY d{&m,1,nullptr,nullptr,verify};
  auto decision=uke_boot_decide(&d);
  if(decision.action!=UKE_BOOT_ANDROID || decision.reason!=UKE_NO_ESP)++errors;
  g.count=1;g.esps[0].guid[0]=1;g.named_esp_mask=1;
  decision=uke_boot_decide(&d);if(decision.action!=UKE_BOOT_MANAGER)++errors;
  uke_manager_return(&decision);if(decision.action!=UKE_BOOT_ANDROID)++errors;
  const char cfg[]="schema=1\nprofile=uke-fixture\nmanager=refind\nsha256=0000000000000000000000000000000000000000000000000000000000000000\n";
  UKE_BOOT_CONFIG config{};
  if(uke_config_parse(cfg,sizeof(cfg)-1,"uke-fixture",&config)!=UKE_OK)++errors;
  if(uke_config_parse(cfg,sizeof(cfg)-1,"wrong-profile",&config)!=UKE_WRONG_PROFILE)++errors;
  UKE_BUTTONS b{};UKE_KEY_EVENT e[UKE_KEY_COUNT];
  uke_buttons_init(&b,0,0);uke_buttons_poll(&b,10,4,e);uke_buttons_poll(&b,40,4,e);
  if(e[2]!=UKE_EVENT_NONE)++errors;
  uke_buttons_poll(&b,100,0,e);uke_buttons_poll(&b,130,0,e);if(e[2]!=UKE_EVENT_SELECT)++errors;
  const char passed[]="AArch64 core fixture: SHA/CRC/config/policy/buttons passed; no firmware or hardware execution\n";
  const char failed[]="AArch64 core fixture failed\n";
  const char* message=errors ? failed : passed;
  const long size=errors ? static_cast<long>(sizeof(failed)-1) : static_cast<long>(sizeof(passed)-1);
  syscall3(64,1,reinterpret_cast<long>(message),size);
  syscall3(93,errors ? 1 : 0,0,0);
  for(;;) {}
}
