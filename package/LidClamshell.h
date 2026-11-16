// SPDX-License-Identifier: BSD-2-Clause-Patent
// macOS's power manager follows the keyboard cover's lid (KeyboardProbe.cpp publishLid). Told that the clamshell closed, the root
// domain notifies its interest clients, and WindowServer turns the built-in display off; it would also sleep the system, but this
// board's device tree sets /defaults sleep-disabled, so IOPMrootDomain::privateSleepSystem refuses (checkSystemSleepAllowed). Told
// that it opened, the display comes back. Only changes are told: the hall sensor is also polled once a second, and the root domain
// takes the lid as open until told otherwise, so an open lid at start is not told. Host-tested by tests/lid_clamshell_host.c.
#ifndef SHENG_LID_CLAMSHELL_H
#define SHENG_LID_CLAMSHELL_H
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

enum lid_clamshell_message {LidClamshellNone, LidClamshellClosed, LidClamshellOpened};
struct lid_clamshell {int8_t told;};   // what the power manager was last told: -1 nothing, 0 open, 1 closed
#define LID_CLAMSHELL_INIT {-1}

// The message for the lid as read now, if any.
static inline enum lid_clamshell_message lid_clamshell_step(struct lid_clamshell *s, bool closed)
{
    const int8_t now = closed ? 1 : 0;
    if (s->told == now) return LidClamshellNone;
    if (s->told < 0 && !closed) {s->told = 0; return LidClamshellNone;}
    s->told = now;
    return closed ? LidClamshellClosed : LidClamshellOpened;
}

#endif
