#ifndef OOPS_APPS_NOTIFIER_H
#define OOPS_APPS_NOTIFIER_H

#include <stddef.h>

#include "oops/system.h"

/*
 * notifier: put a line from homebrew on the system's own notification overlay.
 *
 * The one part worth testing off the console is composing the line - what the notification
 * says, built from what the machine is. Firing it (oops_system_notify) and the dialog are the
 * runtime half. So compose is pure and testable; the payload does the firing.
 */

/*
 * Write a status line for `info` into `out` (freestanding, no stdio). Returns the length
 * written, or 0 if the buffer is too small. Shape: "OOPS: Prospero fw 12.40 mem 1784MB".
 */
size_t notifier_compose_status(char *out, size_t max, const oops_system_info_t *info);

#endif /* OOPS_APPS_NOTIFIER_H */
