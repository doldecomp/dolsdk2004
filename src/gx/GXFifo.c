#include <stddef.h>
#include <dolphin/base/PPCArch.h>
#include <dolphin/gx.h>
#include <dolphin/os.h>

#include "__gx.h"

static OSThread* __GXCurrentThread;
static GXBool CPGPLinked;
static BOOL GXOverflowSuspendInProgress;
static GXBreakPtCallback BreakPointCB;
static u32 __GXOverflowCount;

#if DEBUG
static BOOL IsWGPipeRedirected;
#endif

__GXFifoObj* CPUFifo;
__GXFifoObj* GPFifo;
void* __GXCurrentBP;

static void __GXFifoReadEnable(void);
static void __GXFifoReadDisable(void);
static void __GXFifoLink(u8 en);
static void __GXWriteFifoIntEnable(u8 hiWatermarkEn, u8 loWatermarkEn);
static void __GXWriteFifoIntReset(u8 hiWatermarkClr, u8 loWatermarkClr);

#if DEBUG
static char __data_0[] = "[GXOverflowHandler]";
#endif

static void GXOverflowHandler(__OSInterrupt interrupt, OSContext* context) {
#if DEBUG
    if (__gxVerif->verifyLevel > GX_WARN_SEVERE) {
        OSReport(__data_0);
    }
#endif
    ASSERTLINE(377, !GXOverflowSuspendInProgress);

    __GXOverflowCount++;
    __GXWriteFifoIntEnable(0, 1);
    __GXWriteFifoIntReset(1, 0);
    GXOverflowSuspendInProgress = TRUE;

#if DEBUG
    if (__gxVerif->verifyLevel > GX_WARN_SEVERE) {
        OSReport("[GXOverflowHandler Sleeping]");
    }
#endif
    OSSuspendThread(__GXCurrentThread);
}

static void GXUnderflowHandler(s16 interrupt, OSContext* context) {
#if DEBUG
    if (__gxVerif->verifyLevel > GX_WARN_SEVERE) {
        OSReport("[GXUnderflowHandler]");
    }
#endif
    ASSERTLINE(419, GXOverflowSuspendInProgress);

    OSResumeThread(__GXCurrentThread);
    GXOverflowSuspendInProgress = FALSE;
    __GXWriteFifoIntReset(1, 1);
    __GXWriteFifoIntEnable(1, 0);
}

#define SOME_SET_REG_MACRO(reg, size, shift, val)                                                   \
	do {                                                                                            \
		(reg) = (u32)__rlwimi((u32)(reg), (val), (shift), (32 - (shift) - (size)), (31 - (shift))); \
	} while (0);

static void GXBreakPointHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;

    SOME_SET_REG_MACRO(__GXData->cpEnable, 1, 5, 0);
    GX_SET_CP_REG(1, __GXData->cpEnable);
    if (BreakPointCB != NULL) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        BreakPointCB();
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
}

static void GXCPInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
    __GXData->cpStatus = GX_GET_CP_REG(0);
    if (GET_REG_FIELD(__GXData->cpEnable, 1, 3) && GET_REG_FIELD(__GXData->cpStatus, 1, 1)) {
        GXUnderflowHandler(interrupt, context);
    }
    if (GET_REG_FIELD(__GXData->cpEnable, 1, 2) && GET_REG_FIELD(__GXData->cpStatus, 1, 0)) {
        GXOverflowHandler(interrupt, context);
    }
    if (GET_REG_FIELD(__GXData->cpEnable, 1, 5) && GET_REG_FIELD(__GXData->cpStatus, 1, 4)) {
        GXBreakPointHandler(interrupt, context);
    }
}

void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    ASSERTMSGLINE(542, realFifo != CPUFifo,     "GXInitFifoBase: fifo is attached to CPU");
    ASSERTMSGLINE(544, realFifo != GPFifo,      "GXInitFifoBase: fifo is attached to GP");
    ASSERTMSGLINE(546, ((u32)base & 0x1F) == 0, "GXInitFifoBase: base must be 32B aligned");
    ASSERTMSGLINE(548, base != NULL,            "GXInitFifoBase: base pointer is NULL");
    ASSERTMSGLINE(550, (size & 0x1F) == 0,      "GXInitFifoBase: size must be 32B aligned");
    ASSERTMSGLINE(552, size >= 0x10000,         "GXInitFifoBase: fifo is not large enough");

    realFifo->base = base;
    realFifo->top = (u8*)base + size - 4;
    realFifo->size = size;
    realFifo->count = 0;
    GXInitFifoLimits(fifo, size - 0x4000, (size >> 1) & ~0x1F);
    GXInitFifoPtrs(fifo, base, base);
}

void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr) {
    __GXFifoObj* realFifo = (__GXFifoObj *)fifo;
    BOOL enabled;

    ASSERTMSGLINE(592, realFifo != CPUFifo,         "GXInitFifoPtrs: fifo is attached to CPU");
    ASSERTMSGLINE(594, realFifo != GPFifo,          "GXInitFifoPtrs: fifo is attached to GP");
    ASSERTMSGLINE(596, ((u32)readPtr & 0x1F) == 0,  "GXInitFifoPtrs: readPtr not 32B aligned");
    ASSERTMSGLINE(598, ((u32)writePtr & 0x1F) == 0, "GXInitFifoPtrs: writePtr not 32B aligned");
    ASSERTMSGLINE(601, realFifo->base <= readPtr && readPtr < realFifo->top,   "GXInitFifoPtrs: readPtr not in fifo range");
    ASSERTMSGLINE(604, realFifo->base <= writePtr && writePtr < realFifo->top, "GXInitFifoPtrs: writePtr not in fifo range");

    enabled = OSDisableInterrupts();
    realFifo->rdPtr = readPtr;
    realFifo->wrPtr = writePtr;
    realFifo->count = (u8*)writePtr - (u8*)readPtr;
    if (realFifo->count < 0) {
        realFifo->count += realFifo->size;
    }
    OSRestoreInterrupts(enabled);
}

void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWatermark, u32 loWatermark) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    ASSERTMSGLINE(641, realFifo != GPFifo,        "GXInitFifoLimits: fifo is attached to GP");
    ASSERTMSGLINE(643, (hiWatermark & 0x1F) == 0, "GXInitFifoLimits: hiWatermark not 32B aligned");
    ASSERTMSGLINE(645, (loWatermark & 0x1F) == 0, "GXInitFifoLimits: loWatermark not 32B aligned");
    ASSERTMSGLINE(647, hiWatermark < realFifo->top - realFifo->base, "GXInitFifoLimits: hiWatermark too large");
    ASSERTMSGLINE(649, loWatermark < hiWatermark, "GXInitFifoLimits: hiWatermark below lo watermark");

    realFifo->hiWatermark = hiWatermark;
    realFifo->loWatermark = loWatermark;
}

#define GX_SET_PI_REG(offset, val) (*(volatile u32*)((volatile u32*)(__piReg) + (offset)) = val)

// NONMATCHING DEBUG
void GXSetCPUFifo(GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;
    BOOL enabled = OSDisableInterrupts();

    CPUFifo = realFifo;
    if (CPUFifo == GPFifo) {
        u32 reg = 0;

        GX_SET_PI_REG(3, (u32)realFifo->base & 0x3FFFFFFF);
        GX_SET_PI_REG(4, (u32)realFifo->top & 0x3FFFFFFF);

        SET_REG_FIELD(691, reg, 21, 5, (u32)realFifo->wrPtr >> 5);
        SET_REG_FIELD(691, reg, 1, 26, 0);
        GX_SET_PI_REG(5, reg);

        CPGPLinked = GX_TRUE;

        __GXWriteFifoIntReset(1, 1);
        __GXWriteFifoIntEnable(1, 0);
        __GXFifoLink(1);
    } else {
        u32 reg;

        if (CPGPLinked) {
            __GXFifoLink(0);
            CPGPLinked = GX_FALSE;
        }

        __GXWriteFifoIntEnable(0, 0);
        reg = 0;
        GX_SET_PI_REG(3, (u32)realFifo->base & 0x3FFFFFFF);
        GX_SET_PI_REG(4, (u32)realFifo->top & 0x3FFFFFFF);
        SET_REG_FIELD(726, reg, 21, 5, (u32)realFifo->wrPtr >> 5);
        SET_REG_FIELD(726, reg, 1, 26, 0);
        GX_SET_PI_REG(5, reg);
    }

    PPCSync();
    OSRestoreInterrupts(enabled);
}

void GXSetGPFifo(GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;
    BOOL enabled = OSDisableInterrupts();

    __GXFifoReadDisable();
    __GXWriteFifoIntEnable(0, 0);
    GPFifo = realFifo;

    GX_SET_CP_REG(16, (u32)realFifo->base & 0xFFFF);
    GX_SET_CP_REG(18, (u32)realFifo->top & 0xFFFF);
    GX_SET_CP_REG(24, realFifo->count & 0xFFFF);
    GX_SET_CP_REG(26, (u32)realFifo->wrPtr & 0xFFFF);
    GX_SET_CP_REG(28, (u32)realFifo->rdPtr & 0xFFFF);
    GX_SET_CP_REG(20, (u32)realFifo->hiWatermark & 0xFFFF);
    GX_SET_CP_REG(22, (u32)realFifo->loWatermark & 0xFFFF);
    GX_SET_CP_REG(17, ((u32)realFifo->base & 0x3FFFFFFF) >> 16);
    GX_SET_CP_REG(19, ((u32)realFifo->top & 0x3FFFFFFF) >> 16);
    GX_SET_CP_REG(25, realFifo->count >> 16);
    GX_SET_CP_REG(27, ((u32)realFifo->wrPtr & 0x3FFFFFFF) >> 16);
    GX_SET_CP_REG(29, ((u32)realFifo->rdPtr & 0x3FFFFFFF) >> 16);
    GX_SET_CP_REG(21, (u32)realFifo->hiWatermark >> 16);
    GX_SET_CP_REG(23, (u32)realFifo->loWatermark >> 16);

    PPCSync();

    if (CPUFifo == GPFifo) {
        CPGPLinked = GX_TRUE;
        __GXWriteFifoIntEnable(1, 0);
        __GXFifoLink(1);
    } else {
        CPGPLinked = GX_FALSE;
        __GXWriteFifoIntEnable(0, 0);
        __GXFifoLink(0);
    }

    __GXWriteFifoIntReset(1, 1);
    __GXFifoReadEnable();
    OSRestoreInterrupts(enabled);
}

void GXSaveCPUFifo(GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;
    ASSERTMSGLINE(835, realFifo == CPUFifo, "GXSaveCPUFifo: fifo is not attached to CPU");
    GXFlush();
    __GXSaveCPUFifoAux(realFifo);
}

#define SOME_MACRO1(fifo) \
do { \
    u32 temp = GX_GET_CP_REG(29) << 16; \
    temp |= GX_GET_CP_REG(28); \
    fifo->rdPtr = OSPhysicalToCached(temp); \
} while (0)

#define SOME_MACRO2(fifo) \
do { \
    u32 temp = GX_GET_CP_REG(25) << 16; \
    temp |= GX_GET_CP_REG(24); \
    fifo->count = temp; \
} while (0)

void __GXSaveCPUFifoAux(__GXFifoObj* realFifo) {
    BOOL enabled = OSDisableInterrupts();

    realFifo->base = OSPhysicalToCached(GX_GET_PI_REG(3));
    realFifo->top = OSPhysicalToCached(GX_GET_PI_REG(4));
    realFifo->wrPtr = OSPhysicalToCached(GX_GET_PI_REG(5) & 0xFBFFFFFF);
    if (CPGPLinked) {
        SOME_MACRO1(realFifo);
        SOME_MACRO2(realFifo);
    } else {
        realFifo->count = (u8*)realFifo->wrPtr - (u8*)realFifo->rdPtr;
        if (realFifo->count < 0)
            realFifo->count += realFifo->size;
    }
    OSRestoreInterrupts(enabled);
}

void GXSaveGPFifo(GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;
    u32 cpStatus;
    u8 readIdle;
    u32 temp;

    ASSERTMSGLINE(908, realFifo == GPFifo, "GXSaveGPFifo: fifo is not attached to GP");
    cpStatus = *(u16*)__cpReg;
    readIdle = GET_REG_FIELD(cpStatus, 1, 2);
    ASSERTMSGLINE(915, readIdle, "GXSaveGPFifo: GP is not idle");

    SOME_MACRO1(realFifo);
    SOME_MACRO2(realFifo);
}

void GXGetGPStatus(GXBool* overhi, GXBool* underlow, GXBool* readIdle, GXBool* cmdIdle, GXBool* brkpt) {
    __GXData->cpStatus = GX_GET_CP_REG(0);
    *overhi   = GET_REG_FIELD(__GXData->cpStatus, 1, 0);
    *underlow = (int)GET_REG_FIELD(__GXData->cpStatus, 1, 1);
    *readIdle = (int)GET_REG_FIELD(__GXData->cpStatus, 1, 2);
    *cmdIdle  = (int)GET_REG_FIELD(__GXData->cpStatus, 1, 3);
    *brkpt    = (int)GET_REG_FIELD(__GXData->cpStatus, 1, 4);
}

void GXGetFifoStatus(GXFifoObj* fifo, GXBool* overhi, GXBool* underflow, u32* fifoCount, GXBool* cpuWrite, GXBool* gpRead, GXBool* fifowrap) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    *underflow = GX_FALSE;
    *overhi    = GX_FALSE;
    *fifoCount = 0;
    *fifowrap  = GX_FALSE;

    if (realFifo == GPFifo) {
        SOME_MACRO1(realFifo);
        SOME_MACRO2(realFifo);
    }

    if (realFifo == CPUFifo) {
        GXFlush();
        __GXSaveCPUFifoAux(realFifo);
        *fifowrap = (int)GET_REG_FIELD(GX_GET_PI_REG(5), 1, 26);
    }

    *overhi    = (realFifo->count > realFifo->hiWatermark);
    *underflow = (realFifo->count < realFifo->loWatermark);
    *fifoCount = (realFifo->count);
    *cpuWrite  = (CPUFifo == realFifo);
    *gpRead    = (GPFifo == realFifo);
}

void GXGetFifoPtrs(GXFifoObj* fifo, void** readPtr, void** writePtr) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    if (realFifo == CPUFifo) {
        realFifo->wrPtr = OSPhysicalToCached(GX_GET_PI_REG(5) & 0xFBFFFFFF);
    }

    if (realFifo == GPFifo) {
        SOME_MACRO1(realFifo);
        SOME_MACRO2(realFifo);
    } else {
        realFifo->count = (u8*)realFifo->wrPtr - (u8*)realFifo->rdPtr;
        if (realFifo->count < 0) {
            realFifo->count += realFifo->size;
        }
    }

    *readPtr = realFifo->rdPtr;
    *writePtr = realFifo->wrPtr;
}

void* GXGetFifoBase(const GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    return realFifo->base;
}

u32 GXGetFifoSize(const GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    return realFifo->size;
}

void GXGetFifoLimits(const GXFifoObj* fifo, u32* hi, u32* lo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    *hi = realFifo->hiWatermark;
    *lo = realFifo->loWatermark;
}

GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback cb) {
    GXBreakPtCallback oldcb = BreakPointCB;
    BOOL enabled = OSDisableInterrupts();

    BreakPointCB = cb;
    OSRestoreInterrupts(enabled);
    return oldcb;
}

void* __GXCurrentBP;

void GXEnableBreakPt(void* break_pt) {
    BOOL enabled = OSDisableInterrupts();

    __GXFifoReadDisable();
    GX_SET_CP_REG(30, (u32)break_pt);
    GX_SET_CP_REG(31, ((u32)break_pt >> 16) & 0x3FFF);
    SOME_SET_REG_MACRO(__GXData->cpEnable, 1, 1, 1);
    SOME_SET_REG_MACRO(__GXData->cpEnable, 1, 5, 1);
    GX_SET_CP_REG(1, __GXData->cpEnable);
    __GXCurrentBP = break_pt;
    __GXFifoReadEnable();
    OSRestoreInterrupts(enabled);
}

void GXDisableBreakPt(void) {
    BOOL enabled = OSDisableInterrupts();

    SOME_SET_REG_MACRO(__GXData->cpEnable, 1, 1, 0);
    SOME_SET_REG_MACRO(__GXData->cpEnable, 1, 5, 0);
    GX_SET_CP_REG(1, __GXData->cpEnable);
    __GXCurrentBP = NULL;
    OSRestoreInterrupts(enabled);
}

void __GXFifoInit(void) {
    __OSSetInterruptHandler(0x11, GXCPInterruptHandler);
    __OSUnmaskInterrupts(0x4000);
    __GXCurrentThread = OSGetCurrentThread();
    GXOverflowSuspendInProgress = FALSE;
    CPUFifo = NULL;
    GPFifo = NULL;
}

static void __GXFifoReadEnable(void) {
    SET_REG_FIELD(0, __GXData->cpEnable, 1, 0, 1);
    GX_SET_CP_REG(1, __GXData->cpEnable);
}

static void __GXFifoReadDisable(void) {
    SET_REG_FIELD(0, __GXData->cpEnable, 1, 0, 0);
    GX_SET_CP_REG(1, __GXData->cpEnable);
}

static void __GXFifoLink(u8 en) {
    SET_REG_FIELD(1242, __GXData->cpEnable, 1, 4, (en != 0) ? 1 : 0);
    GX_SET_CP_REG(1, __GXData->cpEnable);
}

static void __GXWriteFifoIntEnable(u8 hiWatermarkEn, u8 loWatermarkEn) {
    SET_REG_FIELD(1264, __GXData->cpEnable, 1, 2, hiWatermarkEn);
    SET_REG_FIELD(1265, __GXData->cpEnable, 1, 3, loWatermarkEn);
    GX_SET_CP_REG(1, __GXData->cpEnable);
}

static void __GXWriteFifoIntReset(u8 hiWatermarkClr, u8 loWatermarkClr) {
    SET_REG_FIELD(1288, __GXData->cpClr, 1, 0, hiWatermarkClr);
    SET_REG_FIELD(1289, __GXData->cpClr, 1, 1, loWatermarkClr);
    GX_SET_CP_REG(2, __GXData->cpClr);
}

void __GXInsaneWatermark(void) {
    __GXFifoObj* realFifo = GPFifo;

    realFifo->hiWatermark = realFifo->loWatermark + 512;
    GX_SET_CP_REG(20, (realFifo->hiWatermark & 0x3FFFFFFF) & 0xFFFF);
    GX_SET_CP_REG(21, (realFifo->hiWatermark & 0x3FFFFFFF) >> 16);
}

void __GXCleanGPFifo(void) {
    GXFifoObj dummyFifo;
    GXFifoObj* gpFifo;
    GXFifoObj* cpuFifo;
    void* base;

    gpFifo = GXGetGPFifo();
    if (gpFifo == (GXFifoObj*)NULL)
        return;

    cpuFifo = GXGetCPUFifo();
    base = GXGetFifoBase(gpFifo);

    dummyFifo = *gpFifo;
    GXInitFifoPtrs(&dummyFifo, base, base);
    GXSetGPFifo(&dummyFifo);
    if (cpuFifo == gpFifo) {
        GXSetCPUFifo(&dummyFifo);
    }
    GXInitFifoPtrs(gpFifo, base, base);
    GXSetGPFifo(gpFifo);
    if (cpuFifo == gpFifo) {
        GXSetCPUFifo(cpuFifo);
    }
}

OSThread* GXSetCurrentGXThread(void) {
    BOOL enabled;
    OSThread* prev;

    enabled = OSDisableInterrupts();
    prev = __GXCurrentThread;
    ASSERTMSGLINE(1377, !GXOverflowSuspendInProgress, "GXSetCurrentGXThread: Two threads cannot generate GX commands at the same time!");
    __GXCurrentThread = OSGetCurrentThread();
    OSRestoreInterrupts(enabled);
    return prev;
}

OSThread* GXGetCurrentGXThread(void) {
    return __GXCurrentThread;
}

GXFifoObj* GXGetCPUFifo(void) {
    return (GXFifoObj*)CPUFifo;
}

GXFifoObj* GXGetGPFifo(void) {
    return (GXFifoObj*)GPFifo;
}

u32 GXGetOverflowCount(void) {
    return __GXOverflowCount;
}

u32 GXResetOverflowCount(void) {
    u32 oldcount;

    oldcount = __GXOverflowCount;
    __GXOverflowCount = 0;
    return oldcount;
}

// NONMATCHING
volatile void* GXRedirectWriteGatherPipe(void* ptr) {
    u32 reg = 0;
    BOOL enabled = OSDisableInterrupts();

    CHECK_GXBEGIN(1493, "GXRedirectWriteGatherPipe");
    ASSERTLINE(1494, OFFSET(ptr, 32) == 0);
    ASSERTLINE(1496, !IsWGPipeRedirected);

#if DEBUG
    IsWGPipeRedirected = TRUE;
#endif

    GXFlush();
    while (PPCMfwpar() & 1) {}
    PPCMtwpar((u32)OSUncachedToPhysical((void*)GXFIFO_ADDR));
    if (CPGPLinked) {
        __GXFifoLink(0);
        __GXWriteFifoIntEnable(0, 0);
    }
    CPUFifo->wrPtr = OSPhysicalToCached(GX_GET_PI_REG(5) & 0xFBFFFFFF);
    GX_SET_PI_REG(3, 0);
    GX_SET_PI_REG(4, 0x04000000);
    SET_REG_FIELD(1527, reg, 21, 5, ((u32)ptr & 0x3FFFFFFF) >> 5);
    reg &= 0xFBFFFFFF;
    GX_SET_PI_REG(5, reg);

    PPCSync();
    OSRestoreInterrupts(enabled);
    return (volatile void *)GXFIFO_ADDR;
}

// NONMATCHING
void GXRestoreWriteGatherPipe(void) {
    u32 reg = 0;
    u32 i;
    BOOL enabled;

    ASSERTLINE(1552, IsWGPipeRedirected);

#if DEBUG
    IsWGPipeRedirected = FALSE;
#endif

    enabled = OSDisableInterrupts();
    for (i = 0; i < 31; i++) {
        GXWGFifo.u8 = 0;
    }

    PPCSync();
    while (PPCMfwpar() & 1) {}
    PPCMtwpar((u32)OSUncachedToPhysical((void *)GXFIFO_ADDR));
    GX_SET_PI_REG(3, (u32)CPUFifo->base & 0x3FFFFFFF);
    GX_SET_PI_REG(4, (u32)CPUFifo->top & 0x3FFFFFFF);
    SET_REG_FIELD(1578, reg, 21, 5, ((u32)CPUFifo->wrPtr & 0x3FFFFFFF) >> 5);
    reg &= 0xFBFFFFFF;
    GX_SET_PI_REG(5, reg);
    if (CPGPLinked) {
        __GXWriteFifoIntReset(1, 1);
        __GXWriteFifoIntEnable(1, 0);
        __GXFifoLink(1);
    }

    PPCSync();
    OSRestoreInterrupts(enabled);
}
