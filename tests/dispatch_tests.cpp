// SPDX-License-Identifier: MIT
#include "uke/boot_core.h"
#include <cstdio>
#include <cstdlib>
struct Fixture {
  bool ready = true;
  unsigned verified = 0, launched = 0, resumed = 0;
  UKE_STATUS verified_status = UKE_OK, launched_status = UKE_MANAGER_RETURNED;
  UKE_STATUS resumed_status = UKE_BACKEND_UNAVAILABLE, reason = UKE_OK;
  static int is_ready(void* p) { return static_cast<Fixture*>(p)->ready; }
  static UKE_STATUS verify(void* p, size_t, const UKE_PARTITION*, UKE_BOOT_CONFIG* config) {
    auto& f = *static_cast<Fixture*>(p); ++f.verified;
    *config = {}; config->manager = UKE_MANAGER_REFIND; return f.verified_status;
  }
  static UKE_STATUS launch(void* p, const UKE_BOOT_DECISION*) {
    auto& f = *static_cast<Fixture*>(p); ++f.launched; return f.launched_status;
  }
  static UKE_STATUS resume(void* p, UKE_STATUS reason) {
    auto& f = *static_cast<Fixture*>(p); ++f.resumed; f.reason = reason; return f.resumed_status;
  }
};
static void require(bool value, const char* message) {
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
  for (unsigned mode = 0; mode < 7; ++mode) {
    Fixture f;
    UKE_GPT_RESULT gpt {}; gpt.count = mode == 1 ? 0 : 1;
    gpt.named_esp_mask = 1;
    UKE_MEDIA media {1, &gpt, UKE_OK};
    UKE_DISCOVERY discovery {&media, 1, nullptr, &f, Fixture::verify};
    UKE_DISPATCH_BACKEND backend {&f, Fixture::is_ready, Fixture::launch, Fixture::resume};
    UKE_BOOT_DECISION decision {};
    if (mode == 0) f.ready = false;
    if (mode == 3) f.verified_status = UKE_HASH_MISMATCH;
    if (mode == 4) f.launched_status = UKE_INVALID_IMAGE;
    if (mode == 5) f.resumed_status = UKE_OK;
    if (mode == 6) media.admitted_internal = 0;
    require(uke_boot_dispatch(&discovery, &backend, &decision) == UKE_BACKEND_UNAVAILABLE, "returned backend is failure");
    if (mode == 0) require(f.verified == 0 && f.launched == 0 && f.resumed == 0, "return gate precedes discovery");
    else {
      require(f.resumed == 1, "Android continuation called exactly once");
      require(decision.action == UKE_BOOT_ANDROID, "returned manager requests Android");
      if (mode == 1 || mode == 3 || mode == 6) require(f.launched == 0, "ineligible candidate never launched");
      else require(f.launched == 1, "eligible candidate launched once");
      if (mode == 2 || mode == 5) require(f.reason == UKE_MANAGER_RETURNED, "returned manager reason retained");
      if (mode == 4) require(f.reason == UKE_INVALID_IMAGE, "load authentication error retained");
    }
  }
  std::puts("dispatch: 7/7 lifecycle scenarios passed; callbacks are fixtures");
}
