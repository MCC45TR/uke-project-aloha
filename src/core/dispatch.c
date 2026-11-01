/* SPDX-License-Identifier: MIT */
#include "uke/boot_core.h"
UKE_STATUS uke_boot_dispatch(const UKE_DISCOVERY *discovery,
                            const UKE_DISPATCH_BACKEND *backend,
                            UKE_BOOT_DECISION *decision) {
  UKE_STATUS returned;
  if (!decision) return UKE_INVALID_ARGUMENT;
  *decision = (UKE_BOOT_DECISION){0};
  decision->reason = UKE_BACKEND_UNAVAILABLE;
  if (!backend || !backend->android_ready || !backend->launch_manager ||
      !backend->resume_android || !backend->android_ready(backend->context))
    return UKE_BACKEND_UNAVAILABLE;
  *decision = uke_boot_decide(discovery);
  if (decision->action == UKE_BOOT_MANAGER) {
    returned = backend->launch_manager(backend->context, decision);
    uke_manager_return(decision);
    if (returned != UKE_OK && returned != UKE_MANAGER_RETURNED) decision->reason = returned;
  }
  returned = backend->resume_android(backend->context, decision->reason);
  return returned == UKE_OK ? UKE_BACKEND_UNAVAILABLE : returned;
}
