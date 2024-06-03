#ifndef _DOLPHIN_CARDBIOS_H_
#define _DOLPHIN_CARDBIOS_H_

void CARDInit(void);
s32 CARDGetResultCode(s32 chan);
s32 CARDFreeBlocks(s32 chan, s32 *byteNotUsed, s32 *filesNotUsed);
long CARDGetEncoding(long chan, unsigned short * encode);
long CARDGetMemSize(long chan, unsigned short * size);
s32 CARDGetSectorSize(s32 chan, u32 *size);
const DVDDiskID* CARDGetDiskID(s32 chan);
s32 CARDSetDiskID(s32 chan, const DVDDiskID* diskID);
BOOL CARDSetFastMode(BOOL enable);
BOOL CARDGetFastMode(void);
s32 CARDGetCurrentMode(s32 chan, u32* mode);

#endif // _DOLPHIN_CARDBIOS_H_
