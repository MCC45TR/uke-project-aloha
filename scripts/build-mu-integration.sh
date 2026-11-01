#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 1 ]] || { printf 'Usage: build-mu-integration.sh CLANG_BIN_DIR\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
base_import="$component/src/upstream/mu_basecore-562c0927ef7bfc65d26d9caf322e569ccb23b6a9"
[[ -f $base_import/MdePkg/MdePkg.dec && -x /usr/bin/python3.13 && -x $compiler/clang ]]
output="$component/build/mu-integration"
mkdir -p -- "$output"
base="$output/BaseCore"
[[ -d $base ]] || cp -a -- "$base_import" "$base"
mipisyst="$component/src/upstream/public-mipi-sys-t-370b5944c046bab043dd8b133727b2135af7747a"
[[ -d $mipisyst/library/include ]]
if [[ ! -L $base/MdePkg/Library/MipiSysTLib/mipisyst ]]; then
  rmdir -- "$base/MdePkg/Library/MipiSysTLib/mipisyst"
  ln -s -- "$mipisyst" "$base/MdePkg/Library/MipiSysTLib/mipisyst"
fi
uuid_rpm="$component/referances/host-rpms/libuuid-devel-2.42.4-2.fc46.x86_64.rpm"
[[ $(sha256sum -- "$uuid_rpm") == '10cdaaef2d196edc3f56ef2311f92cf28cb3b50972da12689be4ada3c8f2d8f5 '* ]]
rpmkeys --checksig "$uuid_rpm" > "$output/host-rpm-signature.txt"
host_sysroot="$output/host-sysroot"
mkdir -p -- "$host_sysroot"
rpm2cpio "$uuid_rpm" | cpio -idu --quiet --no-absolute-filenames --directory="$host_sysroot"
cp -- /usr/lib64/libuuid.so.1.3.0 "$host_sysroot/usr/lib64/libuuid.so.1.3.0"
tools="$output/BaseTools"
if [[ ! -d $tools ]]; then cp -a -- "$base/BaseTools" "$tools"; fi
[[ -e $output/MdePkg ]] || ln -s -- "$base/MdePkg" "$output/MdePkg"
export WORKSPACE="$component" PACKAGES_PATH="$base:$component"
export EDK_TOOLS_PATH="$tools" PYTHON_COMMAND=/usr/bin/python3.13
export CLANGPDB_BIN="$compiler/" PATH="$tools/BinWrappers/PosixLike:$tools/Source/C/bin:$compiler:$PATH"
export CONF_PATH="$output/Conf"
mkdir -p -- "$CONF_PATH"
for config in target tools_def build_rule; do
  cp -- "$tools/Conf/$config.template" "$CONF_PATH/$config.txt"
done
{
  "$PYTHON_COMMAND" --version
  sha256sum -- "$PYTHON_COMMAND"
  sha256sum -- "$uuid_rpm" /usr/lib64/libuuid.so.1.3.0
  "$compiler/clang" --version
  sha256sum -- "$compiler/clang" "$compiler/lld-link"
} > "$output/host-tools.txt"
make -C "$tools/Source/C" -f GNUmakefile -j4 APPLICATIONS='GenFw GenSec GenFfs GenFv' \
  BUILD_OPTFLAGS='-O2 -Wno-error=unused-but-set-variable' \
  TOOL_INCLUDE="-I$host_sysroot/usr/include" EXTRA_LDFLAGS="-L$host_sysroot/usr/lib64" \
  > "$output/basetools-build.log" 2>&1
export PYTHONPATH="$tools/Source/Python"
"$PYTHON_COMMAND" "$tools/Source/Python/build/build.py" \
  -p src/platform/UkePlatformPkg/UkeBringup.dsc -a AARCH64 -t CLANGPDB -b RELEASE \
  -n 4 --conf "$CONF_PATH" > "$output/integration-build.log" 2>&1
printf 'Mu INF/DEC/DSC component build passed; no full firmware image.\n'
