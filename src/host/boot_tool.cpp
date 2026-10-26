// SPDX-License-Identifier: MIT
#include "uke/boot_image.hpp"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace {
bool read_file(const char* path, uke::Bytes& bytes) {
  const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) return false;
  struct stat st {};
  bool good = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0 &&
              st.st_size <= 128 * 1024 * 1024;
  if (good) {
    bytes.resize(static_cast<size_t>(st.st_size));
    size_t done = 0;
    while (done < bytes.size()) {
      const auto n = read(fd, bytes.data() + done, bytes.size() - done);
      if (n <= 0) { good = false; break; }
      done += static_cast<size_t>(n);
    }
    uint8_t extra = 0;
    struct stat after {};
    good = good && read(fd, &extra, 1) == 0 && fstat(fd, &after) == 0 &&
           st.st_size == after.st_size && st.st_mtim.tv_sec == after.st_mtim.tv_sec &&
           st.st_mtim.tv_nsec == after.st_mtim.tv_nsec;
  }
  close(fd); return good;
}
bool write_new(const char* path, std::span<const uint8_t> bytes) {
  // A caller-owned directory is required. Never overwrite a backup or follow
  // a final-component symlink, and never open a partition device for writing.
  const std::string name(path);
  const auto slash = name.find_last_of('/');
  const std::string parent = slash == std::string::npos ? "." : slash == 0 ? "/" : name.substr(0, slash);
  const std::string leaf = slash == std::string::npos ? name : name.substr(slash + 1);
  if (leaf.empty() || leaf == "." || leaf == "..") return false;
  const int directory = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (directory < 0) return false;
  const int fd = openat(directory, leaf.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (fd < 0) { close(directory); return false; }
  bool good = true; size_t done = 0;
  while (done < bytes.size()) {
    const auto n = write(fd, bytes.data() + done, bytes.size() - done);
    if (n <= 0) { good = false; break; }
    done += static_cast<size_t>(n);
  }
  good = good && fsync(fd) == 0;
  uke::Bytes readback(bytes.size()); done = 0;
  while (good && done < readback.size()) {
    const auto n = pread(fd, readback.data() + done, readback.size() - done, static_cast<off_t>(done));
    if (n <= 0) { good = false; break; }
    done += static_cast<size_t>(n);
  }
  struct stat opened {}, named {};
  const bool owns_leaf = fstat(fd, &opened) == 0 &&
      fstatat(directory, leaf.c_str(), &named, AT_SYMLINK_NOFOLLOW) == 0 &&
      opened.st_ino == named.st_ino && opened.st_dev == named.st_dev && S_ISREG(named.st_mode);
  good = good && owns_leaf && uke::sha256_hex(readback) == uke::sha256_hex(bytes) && fsync(directory) == 0;
  if (close(fd) != 0) good = false;
  if (!good && owns_leaf) unlinkat(directory, leaf.c_str(), 0);
  close(directory);
  return good;
}
}

int main(int argc, char** argv) {
  uke::Bytes boot, selector, staged, output; std::string error;
  if (argc == 3 && std::string_view(argv[1]) == "inspect") {
    uke::BootLayout layout;
    if (!read_file(argv[2], boot)) { std::fputs("Regular, non-symlink boot file required.\n", stderr); return 2; }
    if (!uke::inspect_boot(boot, layout, error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    std::printf("boot-header=4\nkernel-bytes=%u\nimage-bytes=%llu\navb-footer=%s\nembedded-vbmeta=%u\nsha256=%s\n",
                layout.kernel_bytes, static_cast<unsigned long long>(layout.image_bytes),
                layout.avb_footer ? "present-not-authenticated" : "absent", layout.embedded_vbmeta, uke::sha256_hex(boot).c_str());
    return 0;
  }
  const bool stage = argc == 7 && std::string_view(argv[1]) == "stage-android-only" &&
                     std::string_view(argv[6]) == "--unsigned-development";
  const bool restore = argc == 7 && std::string_view(argv[1]) == "restore-android-only";
  const bool probe_stage = argc == 8 && std::string_view(argv[1]) == "stage-sec-probe" &&
                           std::string_view(argv[7]) == "--unsigned-development";
  const bool probe_restore = argc == 8 && std::string_view(argv[1]) == "restore-sec-probe";
  if (stage || restore || probe_stage || probe_restore) {
    if (!read_file(argv[2], boot) || !read_file(argv[3], selector) ||
        ((restore || probe_restore) && !read_file(argv[4], staged))) {
      std::fputs("Only bounded regular, non-symlink input files are accepted.\n", stderr); return 2;
    }
    const bool ok = probe_stage
      ? uke::stage_sec_probe(boot, selector, argv[4], argv[5], output, error)
      : probe_restore
      ? uke::restore_sec_probe(boot, staged, selector, argv[5], argv[6], output, error)
      : stage
      ? uke::stage_android_continuation(boot, selector, argv[4], output, error)
      : uke::restore_android_continuation(boot, staged, selector, argv[5], output, error);
    if (!ok) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    const char* path = probe_stage ? argv[6] : probe_restore ? argv[7] : stage ? argv[5] : argv[6];
    if (!write_new(path, output)) { std::fputs("Cannot create and verify new output file.\n", stderr); return 2; }
    std::printf("output-sha256=%s\nscope=%s\nuefi-payload=absent\ninstallable=no\n",
                uke::sha256_hex(output).c_str(), probe_stage || probe_restore
                  ? "raw-sec-entry-development" : "offline-android-continuation-development");
    return 0;
  }
  std::fputs("Usage: uke-boot-tool inspect BOOT\n"
             "       uke-boot-tool stage-android-only BOOT SELECTOR STOCK_SHA256 OUT --unsigned-development\n"
             "       uke-boot-tool restore-android-only BACKUP SELECTOR STAGED STOCK_SHA256 OUT\n"
             "       uke-boot-tool stage-sec-probe BOOT PROBE BOOT_SHA256 PROBE_SHA256 OUT --unsigned-development\n"
             "       uke-boot-tool restore-sec-probe BACKUP PROBE STAGED BOOT_SHA256 PROBE_SHA256 OUT\n"
             "Regular files only. No partition write or usable UEFI installer.\n", stderr);
  return 2;
}
