#ifndef _DOLPHIN_AXART_H_
#define _DOLPHIN_AXART_H_

#include <dolphin/types.h>
#include <dolphin/ax.h>

enum __axart_type {
    AXART_TYPE_NONE,
    AXART_TYPE_3D,
    AXART_TYPE_PANNING,
    AXART_TYPE_ITD,
    AXART_TYPE_SRC,
    AXART_TYPE_PITCH,
    AXART_TYPE_PITCH_ENV,
    AXART_TYPE_PITCH_MOD,
    AXART_TYPE_VOLUME,
    AXART_TYPE_AUX_A_VOLUME,
    AXART_TYPE_AUX_B_VOLUME,
    AXART_TYPE_VOLUME_ENV,
    AXART_TYPE_AUX_A_VOLUME_ENV,
    AXART_TYPE_AUX_B_VOLUME_ENV,
    AXART_TYPE_VOLUME_MOD,
    AXART_TYPE_AUX_A_VOLUME_MOD,
    AXART_TYPE_AUX_B_VOLUME_MOD,
    AXART_TYPE_LPF,

    AXART_TYPE_NUM
};

typedef struct {
    void* next;
    u32 type;
} AXART_ART;

typedef struct {
    // total size: 0x20
    f32* lfo; // offset 0x0, size 0x4
    u32 length; // offset 0x4, size 0x4
    f32 delta; // offset 0x8, size 0x4
    u32 sampleIndex; // offset 0xC, size 0x4
    f32 counter; // offset 0x10, size 0x4
    f32 sample1; // offset 0x14, size 0x4
    f32 sample; // offset 0x18, size 0x4
    f32 output; // offset 0x1C, size 0x4
} AXART_LFO;

typedef struct {

    AXART_ART art;
    f32 hAngle; // offset 0x8, size 0x4
    f32 vAngle; // offset 0xC, size 0x4
    f32 dist; // offset 0x10, size 0x4
    f32 closingSpeed; // offset 0x14, size 0x4
    u32 update; // offset 0x18, size 0x4
    u8 pan; // offset 0x1C, size 0x1
    u8 span; // offset 0x1D, size 0x1
    u8 src; // offset 0x1E, size 0x1
    u16 itdL; // offset 0x20, size 0x2
    u16 itdR; // offset 0x22, size 0x2
    f32 pitch; // offset 0x24, size 0x4
    s32 attenuation;
} AXART_3D;

typedef struct {
    // total size: 0xC
    AXART_ART art;
    u8 pan; // offset 0x8, size 0x1
    u8 span; // offset 0x9, size 0x1
} AXART_PANNING;

typedef struct {
    // total size: 0xC
    AXART_ART art;
    u16 itdL; // offset 0x8, size 0x2
    u16 itdR; // offset 0xA, size 0x2
} AXART_ITD;

typedef struct {
        // total size: 0xC
    AXART_ART art;
    u8 src; // offset 0x8, size 0x1
} AXART_SRC;

typedef struct {
    // total size: 0xC
    AXART_ART art;
    s32 cents; // offset 0x8, size 0x4
} AXART_PITCH;

typedef struct {
    // total size: 0x14
    AXART_ART art;
    s32 delta; // offset 0x8, size 0x4
    s32 target; // offset 0xC, size 0x4
    s32 cents; // offset 0x10, size 0x4
} AXART_PITCH_ENV;

typedef struct {

    AXART_ART art;
    AXART_LFO lfo;
    s32 cents;
} AXART_PITCH_MOD;

typedef struct {
    // total size: 0xC
    AXART_ART art;
    s32 attenuation; // offset 0x8, size 0x4
} AXART_VOLUME;

typedef struct {
    // total size: 0xC
    AXART_ART art;
    s32 attenuation; // offset 0x8, size 0x4
} AXART_AUXA_VOLUME;

typedef struct {
    // total size: 0xC
    AXART_ART art;
    s32 attenuation; // offset 0x8, size 0x4
} AXART_AUXB_VOLUME;

typedef struct {
    // total size: 0x14
    AXART_ART art;
    s32 delta; // offset 0x8, size 0x4
    s32 target; // offset 0xC, size 0x4
    s32 attenuation; // offset 0x10, size 0x4
} AXART_VOLUME_ENV;

typedef struct {
    // total size: 0x14
    AXART_ART art;
    s32 delta; // offset 0x8, size 0x4
    s32 target; // offset 0xC, size 0x4
    s32 attenuation; // offset 0x10, size 0x4
} AXART_AUXA_VOLUME_ENV;

typedef struct {
    // total size: 0x14
    AXART_ART art;
    s32 delta; // offset 0x8, size 0x4
    s32 target; // offset 0xC, size 0x4
    s32 attenuation; // offset 0x10, size 0x4
} AXART_AUXB_VOLUME_ENV;

typedef struct {

    AXART_ART art;
    AXART_LFO lfo;
    s32 attenuation;
} AXART_VOLUME_MOD;

typedef struct {

    AXART_ART art;
    AXART_LFO lfo;
    s32 attenuation;
} AXART_AUXA_VOLUME_MOD;

typedef struct {
    AXART_ART art;
    AXART_LFO lfo;
    s32 attenuation;
} AXART_AUXB_VOLUME_MOD;

typedef struct {
    AXART_ART art;
    u32 initLPF;
    u32 frequency;
    u32 update;
} AXART_LPF;

typedef struct {
    void* next;
    void* prev;
    AXVPB* axvpb;
    f32 sampleRate;
    AXART_ART* articulators;
} AXART_SOUND;

#define AXART_SINE_CNT 64
extern f32 AXARTSine[AXART_SINE_CNT];

// axart.c
void AXARTInit(void);
void AXARTQuit(void);
void AXARTServiceSounds(void);
void AXARTAddSound(AXART_SOUND* sound);
void AXARTRemoveSound(AXART_SOUND* sound);
void AXARTInitLfo(AXART_LFO* lfo, f32* samples, u32 length, f32 delta);
void AXARTInitArt3D(AXART_3D* articulator);
void AXARTInitArtPanning(AXART_PANNING* articulator);
void AXARTInitArtItd(AXART_ITD* articulator);
void AXARTInitArtSrctype(AXART_SRC* articulator);
void AXARTInitArtPitch(AXART_PITCH* articulator);
void AXARTInitArtPitchEnv(AXART_PITCH_ENV* articulator);
void AXARTInitArtPitchMod(AXART_PITCH_MOD* articulator);
void AXARTInitArtVolume(AXART_VOLUME* articulator);
void AXARTInitArtAuxAVolume(AXART_AUXA_VOLUME* articulator);
void AXARTInitArtAuxBVolume(AXART_AUXB_VOLUME* articulator);
void AXARTInitArtVolumeEnv(AXART_VOLUME_ENV* articulator);
void AXARTInitArtAuxAVolumeEnv(AXART_AUXA_VOLUME_ENV* articulator);
void AXARTInitArtAuxBVolumeEnv(AXART_AUXB_VOLUME_ENV* articulator);
void AXARTInitArtVolumeMod(AXART_VOLUME_MOD* articulator);
void AXARTInitArtAuxAVolumeMod(AXART_AUXA_VOLUME_MOD* articulator);
void AXARTInitArtAuxBVolumeMod(AXART_AUXB_VOLUME_MOD* articulator);
void AXARTInitArtLpf(AXART_LPF* articulator);

// axart3d.c
void AXARTSet3DDistanceScale(f32 scale);
void AXARTSet3DDopplerScale(f32 scale);
void AXART3DSound(AXART_3D* articulator);

// axartcents.c
f32 AXARTCents(s32 cents);

// axartenv.c
void AXARTPitchEnv(AXART_PITCH_ENV* articulator);
void AXARTVolumeEnv(AXART_VOLUME_ENV* articulator);

// axartlfo.c
void AXARTLfo(AXART_LFO* lfo);

// axartsound.c
void AXARTServiceSound(AXART_SOUND* sound);
void AXARTAddArticulator(AXART_SOUND* sound, AXART_ART* articulator);

// axartlpf
void AXARTLpf(AXART_LPF*, AXVPB*);

#endif // _DOLPHIN_AXART_H_
