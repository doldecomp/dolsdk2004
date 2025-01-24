#include <dolphin.h>
#include <dolphin/mtx.h>
#include "fake_tgmath.h"

// TODO: A bunch of pointless void casts because of Mtx type-ness.
// Dunno how to resolve this at the moment.

void MTXInitStack(MTXStack *sPtr, u32 numMtx) {
    ASSERTMSGLINE(0x4A, sPtr, "MTXInitStack():  NULL MtxStackPtr 'sPtr' ");
    ASSERTMSGLINE(0x4B, sPtr->stackBase, "MTXInitStack():  'sPtr' contains a NULL ptr to stack memory ");
    ASSERTMSGLINE(0x4C, numMtx, "MTXInitStack():  'numMtx' is 0 ");
    sPtr->numMtx = numMtx;
    sPtr->stackPtr = 0;
}

Mtx *MTXPush(MTXStack *sPtr, Mtx m) {
    ASSERTMSGLINE(0x68, sPtr, "MTXPush():  NULL MtxStackPtr 'sPtr' ");
    ASSERTMSGLINE(0x69, sPtr->stackBase, "MTXPush():  'sPtr' contains a NULL ptr to stack memory ");
    ASSERTMSGLINE(0x6A, m, "MTXPush():  NULL MtxPtr 'm' ");
    if (sPtr->stackPtr == NULL) {
        sPtr->stackPtr = sPtr->stackBase;
        MTXCopy((void*)m, (void*)sPtr->stackPtr);
    } else {
        ASSERTMSGLINE(0x79, ((((s32)sPtr->stackPtr - (s32)sPtr->stackBase) / 16) / 3) < (sPtr->numMtx - 1), "MTXPush():  stack overflow ");
        MTXCopy((void*)m, (void*)(sPtr->stackPtr + 1));
        sPtr->stackPtr++;
    }
    return sPtr->stackPtr;
}

Mtx *MTXPushFwd(MTXStack *sPtr, Mtx m) {
    ASSERTMSGLINE(0x9D, sPtr, "MTXPushFwd():  NULL MtxStackPtr 'sPtr' ");
    ASSERTMSGLINE(0x9E, sPtr->stackBase, "MTXPushFwd():  'sPtr' contains a NULL ptr to stack memory ");
    ASSERTMSGLINE(0x9F, m, "MTXPushFwd():  NULL MtxPtr 'm' ");

    if (sPtr->stackPtr == NULL) {
        sPtr->stackPtr = sPtr->stackBase;
        MTXCopy((void*)m, (void*)sPtr->stackPtr);
    } else {
        ASSERTMSGLINE(0xAE, ((((s32)sPtr->stackPtr - (s32)sPtr->stackBase) / 16) / 3) < (sPtr->numMtx - 1), "MTXPushFwd():  stack overflow");
        MTXConcat((void*)sPtr->stackPtr, (void*)m, (void*)(sPtr->stackPtr + 1));
        sPtr->stackPtr++;
    }
    return sPtr->stackPtr;
}

Mtx *MTXPushInv(MTXStack *sPtr, Mtx m) {
    Mtx mInv;

    ASSERTMSGLINE(0xD8, sPtr, "MTXPushInv():  NULL MtxStackPtr 'sPtr' ");
    ASSERTMSGLINE(0xD9, sPtr->stackBase, "MTXPushInv():  'sPtr' contains a NULL ptr to stack memory ");
    ASSERTMSGLINE(0xDA, m, "MTXPushInv():  NULL MtxPtr 'm' ");
    MTXInverse((void*)m, (void*)&mInv);
    if (sPtr->stackPtr == NULL) {
        sPtr->stackPtr = sPtr->stackBase;
        MTXCopy((void*)&mInv, (void*)sPtr->stackPtr);
    } else {
        ASSERTMSGLINE(0xEC, ((((s32)sPtr->stackPtr - (s32)sPtr->stackBase) / 16) / 3) < (sPtr->numMtx - 1), "MTXPushInv():  stack overflow");
        MTXConcat((void*)&mInv, (void*)sPtr->stackPtr, (void*)(sPtr->stackPtr + 1));
        sPtr->stackPtr++;
    }
    return sPtr->stackPtr;
}

Mtx *MTXPushInvXpose(MTXStack *sPtr, Mtx m) {
    Mtx mIT;

    ASSERTMSGLINE(0x117, sPtr, "MTXPushInvXpose():  NULL MtxStackPtr 'sPtr' ");
    ASSERTMSGLINE(0x118, sPtr->stackBase, "MTXPushInvXpose():  'sPtr' contains a NULL ptr to stack memory ");
    ASSERTMSGLINE(0x119, m, "MTXPushInvXpose():  NULL MtxPtr 'm' ");
    MTXInverse((void*)m, (void*)&mIT);
    MTXTranspose((void*)&mIT, (void*)&mIT);
    if (sPtr->stackPtr == NULL) {
        sPtr->stackPtr = sPtr->stackBase;
        MTXCopy((void*)&mIT, (void*)sPtr->stackPtr);
    } else {
        ASSERTMSGLINE(0x12C, ((((s32)sPtr->stackPtr - (s32)sPtr->stackBase) / 16) / 3) < (sPtr->numMtx - 1), "MTXPushInvXpose():  stack overflow ");
        MTXConcat((void*)sPtr->stackPtr, (void*)&mIT, (void*)(sPtr->stackPtr + 1));
        sPtr->stackPtr++;
    }
    return sPtr->stackPtr;
}

Mtx *MTXPop(MTXStack *sPtr) {
    ASSERTMSGLINE(0x148, sPtr, "MTXPop():  NULL MtxStackPtr 'sPtr' ");
    ASSERTMSGLINE(0x149, sPtr->stackBase, "MTXPop():  'sPtr' contains a NULL ptr to stack memory ");
    if (sPtr->stackPtr == NULL) {
        return NULL;
    }
    if (sPtr->stackBase == sPtr->stackPtr) {
        sPtr->stackPtr = NULL;
        return NULL;
    }
    sPtr->stackPtr--;
    return sPtr->stackPtr;
}

Mtx *MTXGetStackPtr(MTXStack *sPtr) {
    ASSERTMSGLINE(0x16E, sPtr, "MTXGetStackPtr():  NULL MtxStackPtr 'sPtr' ");
    ASSERTMSGLINE(0x16F, sPtr->stackBase, "MTXGetStackPtr():  'sPtr' contains a NULL ptr to stack memory ");
    return sPtr->stackPtr;
}
