// SPDX-License-Identifier: MIT
#include "uke/boot_core.h"
#include <array>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

class RegularFile {
 public:
  explicit RegularFile(const char* path) {
    fd_ = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    struct stat st {};
    if (fd_ < 0 || fstat(fd_, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
      if (fd_ >= 0) close(fd_);
      fd_ = -1;
    } else size_ = static_cast<uint64_t>(st.st_size);
  }
  ~RegularFile() { if (fd_ >= 0) close(fd_); }
  RegularFile(const RegularFile&) = delete;
  RegularFile& operator=(const RegularFile&) = delete;
  bool valid() const { return fd_ >= 0; }
  uint64_t size() const { return size_; }
  static int read(void* context, uint64_t offset, size_t bytes, void* output) {
    auto& file = *static_cast<RegularFile*>(context);
    if (offset > file.size_ || bytes > file.size_ - offset ||
        offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max())) return 0;
    auto* out = static_cast<unsigned char*>(output);
    size_t done = 0;
    while (done < bytes) {
      const auto n = pread(file.fd_, out + done, bytes - done, static_cast<off_t>(offset + done));
      if (n <= 0) return 0;
      done += static_cast<size_t>(n);
    }
    return 1;
  }
 private:
  int fd_ = -1;
  uint64_t size_ = 0;
};

static void guid(const uint8_t* p) {
  // Render canonical GUID order; the core stores the actual GPT bytes.
  std::printf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
              p[3],p[2],p[1],p[0],p[5],p[4],p[7],p[6],p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]);
}

struct FilesystemVerifier {
  const UKE_DISK* disk;
  const char* profile;
  static UKE_STATUS verify(void* opaque, size_t, const UKE_PARTITION* p, UKE_BOOT_CONFIG* out) {
    auto& context = *static_cast<FilesystemVerifier*>(opaque);
    UKE_FAT32 volume {};
    auto status = uke_fat32_open(context.disk, p, &volume);
    if (status != UKE_OK) return status;
    size_t bytes = 0;
    std::vector<uint8_t> image(UKE_MAX_IMAGE);
    return uke_esp_manager_probe(&volume, context.profile, image.data(), image.size(), out, &bytes);
  }
};

int main(int argc, char** argv) {
  if ((argc == 4 && std::string_view(argv[1]) == "gpt") ||
      (argc == 5 && std::string_view(argv[1]) == "boot")) {
    uint32_t block = 0;
    const std::string_view number(argv[3]);
    const auto parsed = std::from_chars(number.data(), number.data() + number.size(), block);
    if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()) return 2;
    RegularFile file(argv[2]);
    if (!file.valid()) { std::fputs("Only a regular, non-symlink image file is accepted.\n", stderr); return 2; }
    UKE_DISK disk {&file, RegularFile::read, file.size(), block};
    UKE_GPT_WORKSPACE workspace {};
    UKE_GPT_RESULT result {};
    const auto s = uke_gpt_scan(&disk, &workspace, &result);
    std::printf("gpt=%s\n", uke_status_name(s));
    if (argc == 5) {
      // This flag admits a caller-supplied disposable image only, never a UFS device.
      UKE_MEDIA media {1,&result,s};
      FilesystemVerifier verifier {&disk,argv[4]};
      UKE_DISCOVERY discovery {&media,1,nullptr,&verifier,FilesystemVerifier::verify};
      const auto decision = uke_boot_decide(&discovery);
      std::printf("boot-policy=%s\nreason=%s\n",
                  decision.action == UKE_BOOT_MANAGER ? "manager-eligible" : "android-requested",
                  uke_status_name(decision.reason));
      if (decision.action == UKE_BOOT_MANAGER)
        std::printf("owned-esp-path=%s\n",uke_manager_path(&decision.config));
      std::puts("scope=regular-file-preflight-only\nandroid-backend=unimplemented\nmanager-executed=no");
      return 0; // A fallback is an ordinary policy outcome, not an execution failure.
    }
    if (s != UKE_OK) {
      std::puts("boot-policy=android-requested\nandroid-backend=unimplemented"); return 1;
    }
    std::printf("esp-count=%u\n", result.count);
    for (uint32_t i = 0; i < result.count; ++i) {
      std::printf("esp-guid="); guid(result.esps[i].guid);
      std::printf(" first-lba=%llu last-lba=%llu\n",
                  static_cast<unsigned long long>(result.esps[i].first_lba),
                  static_cast<unsigned long long>(result.esps[i].last_lba));
    }
    std::puts(result.count ? "manager-eligibility=filesystem-verification-required" : "boot-policy=android-requested");
    return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "manager") {
    RegularFile cfg(argv[2]), image(argv[3]);
    if (!cfg.valid() || !image.valid() || !cfg.size() || cfg.size() > UKE_MAX_CONFIG ||
        !image.size() || image.size() > UKE_MAX_IMAGE) return 2;
    std::array<uint8_t, UKE_MAX_CONFIG> buffer {};
    UKE_BOOT_CONFIG config {};
    if (!RegularFile::read(&cfg, 0, static_cast<size_t>(cfg.size()), buffer.data())) return 2;
    auto s = uke_config_parse(buffer.data(), static_cast<size_t>(cfg.size()), argv[4], &config);
    if (s == UKE_OK) {
      std::vector<uint8_t> payload(static_cast<size_t>(image.size()));
      if (!RegularFile::read(&image, 0, payload.size(), payload.data())) return 2;
      s = uke_image_check(payload.data(), payload.size(), config.sha256);
    }
    std::printf("manager-check=%s\n", uke_status_name(s));
    if (s == UKE_OK) std::printf("owned-esp-path=%s\n", uke_manager_path(&config));
    std::puts("scope=regular-file-preflight-only");
    return s == UKE_OK ? 0 : 1;
  }
  std::fputs("Usage: uke-esp-inspect gpt IMAGE BLOCK_BYTES\n"
             "       uke-esp-inspect boot IMAGE BLOCK_BYTES PROFILE\n"
             "       uke-esp-inspect manager CONFIG EFI_IMAGE PROFILE\n"
             "Read-only regular files only; no filesystem mount or boot dispatch.\n", stderr);
  return 2;
}
