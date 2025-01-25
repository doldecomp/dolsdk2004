#ifndef _DOLPHIN_OSTIMER_H_
#define _DOLPHIN_OSTIMER_H_

#include <dolphin/types.h>

typedef void (*OSTimerCallback)(void);

OSTimerCallback OSSetTimerCallback(OSTimerCallback callback);
void OSInitTimer(u32 time, u32 mode);
void OSStartTimer(void);
void OSStopTimer(void);

#endif // _DOLPHIN_OSTIMER_H_
