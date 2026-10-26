/* SPDX-License-Identifier: MIT */
#include "uke/platform_boot.h"
UKE_STATUS uke_return_register(UKE_RETURN_TRANSACTION *t, void *context,
                               UKE_QUIESCE quiesce, size_t *index) {
  size_t i;
  if (!t || !quiesce || !index || t->stage != UKE_RETURN_IDLE ||
      t->driver_count >= UKE_MAX_QUIESCE_DRIVERS) return UKE_INVALID_ARGUMENT;
  for (i = 0; i < t->driver_count; ++i)
    if (t->drivers[i].context == context && t->drivers[i].quiesce == quiesce) return UKE_INVALID_ARGUMENT;
  *index = t->driver_count;
  t->drivers[t->driver_count++] = (UKE_DRIVER){context, quiesce, 0}; return UKE_OK;
}
UKE_STATUS uke_return_mark_dirty(UKE_RETURN_TRANSACTION *t, size_t index) {
  if (!t || t->stage != UKE_RETURN_IDLE || index >= t->driver_count) return UKE_INVALID_ARGUMENT;
  t->drivers[index].dirty = 1; return UKE_OK;
}
static UKE_STATUS failed(UKE_RETURN_TRANSACTION *t, UKE_STATUS status) {
  t->stage = UKE_RETURN_FAILED;
  t->failure = status == UKE_OK ? UKE_BACKEND_UNAVAILABLE : status;
  return t->failure;
}
UKE_STATUS uke_return_android(UKE_RETURN_TRANSACTION *t, const UKE_RETURN_BACKEND *b) {
  size_t i;
  UKE_STATUS status, first_failure = UKE_OK;
  if (!t || !b || !b->ready || !b->prepare || !b->exit_services || !b->jump_android ||
      t->stage != UKE_RETURN_IDLE || t->driver_count > UKE_MAX_QUIESCE_DRIVERS) return UKE_INVALID_ARGUMENT;
  if (!b->ready(b->context)) return failed(t, UKE_BACKEND_UNAVAILABLE);
  t->stage = UKE_RETURN_QUIESCING;
  /* Try every dirty driver even if one fails, but never jump after failure. */
  for (i = t->driver_count; i > 0; --i) {
    UKE_DRIVER *driver = &t->drivers[i - 1];
    if (!driver->dirty) continue;
    if (!driver->quiesce) status = UKE_BACKEND_UNAVAILABLE;
    else status = driver->quiesce(driver->context);
    if (status == UKE_OK) driver->dirty = 0;
    else if (first_failure == UKE_OK) first_failure = status;
  }
  if (first_failure != UKE_OK) return failed(t, first_failure);
  t->stage = UKE_RETURN_PREPARING;
  status = b->prepare(b->context);
  if (status != UKE_OK) return failed(t, status);
  t->stage = UKE_RETURN_EXITING;
  status = b->exit_services(b->context);
  if (status != UKE_OK) return failed(t, status);
  t->stage = UKE_RETURN_JUMPING;
  return failed(t, b->jump_android(b->context)); // A real jump cannot return.
}
