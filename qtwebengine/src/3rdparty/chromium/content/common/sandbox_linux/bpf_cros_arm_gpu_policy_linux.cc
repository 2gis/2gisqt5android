// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_cros_arm_gpu_policy_linux.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/services/linux_syscalls.h"

using sandbox::SyscallSets;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace content {

namespace {

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureArm() {
#if defined(__arm__)
  return true;
#else
  return false;
#endif
}

void AddArmMaliGpuWhitelist(std::vector<std::string>* read_whitelist,
                            std::vector<std::string>* write_whitelist) {
  // Device file needed by the ARM GPU userspace.
  static const char kMali0Path[] = "/dev/mali0";

  // Devices needed for video decode acceleration on ARM.
  static const char kDevMfcDecPath[] = "/dev/mfc-dec";
  static const char kDevGsc1Path[] = "/dev/gsc1";

  // Devices needed for video encode acceleration on ARM.
  static const char kDevMfcEncPath[] = "/dev/mfc-enc";

  read_whitelist->push_back(kMali0Path);
  read_whitelist->push_back(kDevMfcDecPath);
  read_whitelist->push_back(kDevGsc1Path);
  read_whitelist->push_back(kDevMfcEncPath);

  write_whitelist->push_back(kMali0Path);
  write_whitelist->push_back(kDevMfcDecPath);
  write_whitelist->push_back(kDevGsc1Path);
  write_whitelist->push_back(kDevMfcEncPath);
}

void AddArmGpuWhitelist(std::vector<std::string>* read_whitelist,
                        std::vector<std::string>* write_whitelist) {
  // On ARM we're enabling the sandbox before the X connection is made,
  // so we need to allow access to |.Xauthority|.
  static const char kXAuthorityPath[] = "/home/chronos/.Xauthority";
  static const char kLdSoCache[] = "/etc/ld.so.cache";

  // Files needed by the ARM GPU userspace.
  static const char kLibGlesPath[] = "/usr/lib/libGLESv2.so.2";
  static const char kLibEglPath[] = "/usr/lib/libEGL.so.1";

  read_whitelist->push_back(kXAuthorityPath);
  read_whitelist->push_back(kLdSoCache);
  read_whitelist->push_back(kLibGlesPath);
  read_whitelist->push_back(kLibEglPath);

  AddArmMaliGpuWhitelist(read_whitelist, write_whitelist);
}

class CrosArmGpuBrokerProcessPolicy : public CrosArmGpuProcessPolicy {
 public:
  static sandbox::bpf_dsl::Policy* Create() {
    return new CrosArmGpuBrokerProcessPolicy();
  }
  ~CrosArmGpuBrokerProcessPolicy() override {}

  ResultExpr EvaluateSyscall(int system_call_number) const override;

 private:
  CrosArmGpuBrokerProcessPolicy() : CrosArmGpuProcessPolicy(false) {}
  DISALLOW_COPY_AND_ASSIGN(CrosArmGpuBrokerProcessPolicy);
};

// A GPU broker policy is the same as a GPU policy with open and
// openat allowed.
ResultExpr CrosArmGpuBrokerProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_access:
    case __NR_open:
    case __NR_openat:
      return Allow();
    default:
      return CrosArmGpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace

CrosArmGpuProcessPolicy::CrosArmGpuProcessPolicy(bool allow_shmat)
    : allow_shmat_(allow_shmat) {}

CrosArmGpuProcessPolicy::~CrosArmGpuProcessPolicy() {}

ResultExpr CrosArmGpuProcessPolicy::EvaluateSyscall(int sysno) const {
#if defined(__arm__)
  if (allow_shmat_ && sysno == __NR_shmat)
    return Allow();
#endif  // defined(__arm__)

  switch (sysno) {
#if defined(__arm__)
    // ARM GPU sandbox is started earlier so we need to allow networking
    // in the sandbox.
    case __NR_connect:
    case __NR_getpeername:
    case __NR_getsockname:
    case __NR_sysinfo:
    case __NR_uname:
      return Allow();
    // Allow only AF_UNIX for |domain|.
    case __NR_socket:
    case __NR_socketpair: {
      const Arg<int> domain(0);
      return If(domain == AF_UNIX, Allow()).Else(Error(EPERM));
    }
#endif  // defined(__arm__)
    default:
      // Default to the generic GPU policy.
      return GpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

bool CrosArmGpuProcessPolicy::PreSandboxHook() {
  DCHECK(IsChromeOS() && IsArchitectureArm());
  // Create a new broker process.
  DCHECK(!broker_process());

  std::vector<std::string> read_whitelist_extra;
  std::vector<std::string> write_whitelist_extra;
  // Add ARM-specific files to whitelist in the broker.

  AddArmGpuWhitelist(&read_whitelist_extra, &write_whitelist_extra);
  InitGpuBrokerProcess(CrosArmGpuBrokerProcessPolicy::Create,
                       read_whitelist_extra,
                       write_whitelist_extra);

  const int dlopen_flag = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;

  // Preload the Mali library.
  dlopen("/usr/lib/libmali.so", dlopen_flag);
  // Preload the Tegra V4L2 (video decode acceleration) library.
  dlopen("/usr/lib/libtegrav4l2.so", dlopen_flag);
  // Resetting errno since platform-specific libraries will fail on other
  // platforms.
  errno = 0;

  return true;
}

}  // namespace content
