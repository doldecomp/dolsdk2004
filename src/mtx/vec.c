#include <dolphin.h>
#include <dolphin/mtx.h>
#include "fake_tgmath.h"

// defines to make asm work
#define qr0 0

void C_VECAdd(Vec *a, Vec *b, Vec *c) {
    ASSERTMSGLINE(0x6C, a, "VECAdd():  NULL VecPtr 'a' ");
    ASSERTMSGLINE(0x6D, b, "VECAdd():  NULL VecPtr 'b' ");
    ASSERTMSGLINE(0x6E, c, "VECAdd():  NULL VecPtr 'ab' ");
    c->x = a->x + b->x;
    c->y = a->y + b->y;
    c->z = a->z + b->z;
}

asm void PSVECAdd(register Vec *a, register Vec *b, register Vec *c) {
    psq_l f2, Vec.x(a), 0, qr0
    psq_l f4, Vec.x(b), 0, qr0
    ps_add f6, f2, f4
    psq_st f6, Vec.x(c), 0, qr0
    psq_l f3, Vec.z(a), 1, qr0
    psq_l f5, Vec.z(b), 1, qr0
    ps_add f7, f3, f5
    psq_st f7, Vec.z(c), 1, qr0
}

void C_VECSubtract(Vec *a, Vec *b, Vec *c) {
    ASSERTMSGLINE(0xB1, a, "VECSubtract():  NULL VecPtr 'a' ");
    ASSERTMSGLINE(0xB2, b, "VECSubtract():  NULL VecPtr 'b' ");
    ASSERTMSGLINE(0xB3, c, "VECSubtract():  NULL VecPtr 'a_b' ");
    c->x = a->x - b->x;
    c->y = a->y - b->y;
    c->z = a->z - b->z;
}

asm void PSVECSubtract(register Vec *a, register Vec *b, register Vec *c) {
    psq_l f2, Vec.x(a), 0, qr0
    psq_l f4, Vec.x(b), 0, qr0
    ps_sub f6, f2, f4
    psq_st f6, Vec.x(c), 0, qr0
    psq_l f3, Vec.z(a), 1, qr0
    psq_l f5, Vec.z(b), 1, qr0
    ps_sub f7, f3, f5
    psq_st f7, Vec.z(c), 1, qr0
}

void C_VECScale(Vec *src, Vec *dst, f32 scale) {
    ASSERTMSGLINE(0xF7, src, "VECScale():  NULL VecPtr 'src' ");
    ASSERTMSGLINE(0xF8, dst, "VECScale():  NULL VecPtr 'dst' ");
    dst->x = (src->x * scale);
    dst->y = (src->y * scale);
    dst->z = (src->z * scale);
}

void PSVECScale(register Vec *src, register Vec *dst, register f32 mult) {
    register f32 vxy, vz, rxy, rz;

    asm {
        psq_l vxy, 0x0(src), 0, 0
        psq_l vz, 0x8(src), 1, 0
        ps_muls0 rxy, vxy, mult
        psq_st rxy, 0x0(dst), 0, 0
        ps_muls0 rz, vz, mult
        psq_st rz, 0x8(dst), 1, 0
    }
}

void C_VECNormalize(Vec *src, Vec *unit) {
    f32 mag;

    ASSERTMSGLINE(0x13B, src, "VECNormalize():  NULL VecPtr 'src' ");
    ASSERTMSGLINE(0x13C, unit, "VECNormalize():  NULL VecPtr 'unit' ");
    mag = (src->z * src->z) + ((src->x * src->x) + (src->y * src->y));
    ASSERTMSGLINE(0x141, 0.0f != mag, "VECNormalize():  zero magnitude vector ");
    mag = 1.0f/ sqrtf(mag);
    unit->x = src->x * mag;
    unit->y = src->y * mag;
    unit->z = src->z * mag;
}

void PSVECNormalize(register Vec *vec1, register Vec *dst) {
    register float c_half = 0.5f;
    register float c_three = 3.0f;
    register float v1_xy;
    register float v1_z;
    register float xx_zz;
    register float xx_yy;
    register float sqsum;
    register float rsqrt;
    register float nwork0;
    register float nwork1;

    asm {
        psq_l v1_xy, 0x0(vec1), 0, 0
        ps_mul xx_yy, v1_xy, v1_xy
        psq_l v1_z, 0x8(vec1), 1, 0
        ps_madd xx_zz, v1_z, v1_z, xx_yy
        ps_sum0 sqsum, xx_zz, v1_z, xx_yy
        frsqrte rsqrt, sqsum
        fmuls nwork0, rsqrt, rsqrt
        fmuls nwork1, rsqrt, c_half
        fnmsubs nwork0, nwork0, sqsum, c_three
        fmuls rsqrt, nwork0, nwork1
        ps_muls0 v1_xy, v1_xy, rsqrt
        psq_st v1_xy, 0x0(dst), 0, 0
        ps_muls0 v1_z, v1_z, rsqrt
        psq_st v1_z, 0x8(dst), 1, 0
    }
}

f32 C_VECSquareMag(Vec *v) {
    f32 sqmag;

    ASSERTMSGLINE(0x195, v, "VECMag():  NULL VecPtr 'v' ");

    sqmag = v->z * v->z + ((v->x * v->x) + (v->y * v->y));
    return sqmag;
}

f32 PSVECSquareMag(register Vec *vec1) {
    register f32 vxy, vzz, sqmag;

    asm {
        psq_l vxy, 0x0(vec1), 0, 0
        ps_mul vxy, vxy, vxy
        lfs vzz, 0x8(vec1)
        ps_madd sqmag, vzz, vzz, vxy
        ps_sum0 sqmag, sqmag, vxy, vxy
    }

    return sqmag;
}

f32 C_VECMag(Vec *v) {
    return sqrtf(C_VECSquareMag(v));
}

f32 PSVECMag(const register Vec* v) {
    register f32 vxy, vzz;
    register f32 sqmag, rmag;
    register f32 nwork0, nwork1;
    register f32 c_three, c_half, c_zero;

    c_half = 0.5f;

    asm {
        psq_l vxy, 0x0(v), 0, 0
        ps_mul vxy, vxy, vxy
        lfs vzz, 0x8(v)
        fsubs c_zero, c_half, c_half
        ps_madd sqmag, vzz, vzz, vxy
        ps_sum0 sqmag, sqmag, vxy, vxy
        fcmpu cr0, sqmag, c_zero
        beq L_000005F0
        frsqrte rmag, sqmag
    }

    c_three = 3.0f;

    asm {
        fmuls nwork0, rmag, rmag
        fmuls nwork1, rmag, c_half
        fnmsubs nwork0, nwork0, sqmag, c_three
        fmuls rmag, nwork0, nwork1
        fmuls sqmag, sqmag, rmag
    L_000005F0:
    }

    return sqmag;
}

f32 C_VECDotProduct(Vec *a, Vec *b) {
    f32 dot;

    ASSERTMSGLINE(0x21C, a, "VECDotProduct():  NULL VecPtr 'a' ");
    ASSERTMSGLINE(0x21D, b, "VECDotProduct():  NULL VecPtr 'b' ");
    dot = (a->z * b->z) + ((a->x * b->x) + (a->y * b->y));
    return dot;
}

asm f32 PSVECDotProduct(register Vec *vec1, register Vec *vec2) {
    psq_l f2, Vec.y(vec1), 0, qr0
    psq_l f3, Vec.y(vec2), 0, qr0
    ps_mul f2, f2, f3
    psq_l f5, Vec.x(vec1), 0, qr0
    psq_l f4, Vec.x(vec2), 0, qr0
    ps_madd f3, f5, f4, f2
    ps_sum0 f1, f3, f2, f2
}

void C_VECCrossProduct(Vec *a, Vec *b, Vec *axb) {
    Vec vTmp;

    ASSERTMSGLINE(0x25A, a, "VECCrossProduct():  NULL VecPtr 'a' ");
    ASSERTMSGLINE(0x25B, b, "VECCrossProduct():  NULL VecPtr 'b' ");
    ASSERTMSGLINE(0x25C, axb, "VECCrossProduct():  NULL VecPtr 'axb' ");

    vTmp.x = (a->y * b->z) - (a->z * b->y);
    vTmp.y = (a->z * b->x) - (a->x * b->z);
    vTmp.z = (a->x * b->y) - (a->y * b->x);
    axb->x = vTmp.x;
    axb->y = vTmp.y;
    axb->z = vTmp.z;
}

asm void PSVECCrossProduct(register Vec *vec1, register Vec *vec2, register Vec *dst) {
    psq_l f1, Vec.x(vec2), 0, qr0
    lfs f2, Vec.z(vec1)
    psq_l f0, Vec.x(vec1), 0, qr0
    ps_merge10 f6, f1, f1
    lfs f3, Vec.z(vec2)
    ps_mul f4, f1, f2
    ps_muls0 f7, f1, f0
    ps_msub f5, f0, f3, f4
    ps_msub f8, f0, f6, f7
    ps_merge11 f9, f5, f5
    ps_merge01 f10, f5, f8
    psq_st f9, Vec.x(dst), 1, qr0
    ps_neg f10, f10
    psq_st f10, Vec.y(dst), 0, qr0
}

void C_VECHalfAngle(Vec *a, Vec *b, Vec *half) {
    Vec aTmp;
    Vec bTmp;
    Vec hTmp;

    ASSERTMSGLINE(0x2C3, a, "VECHalfAngle():  NULL VecPtr 'a' ");
    ASSERTMSGLINE(0x2C4, b, "VECHalfAngle():  NULL VecPtr 'b' ");
    ASSERTMSGLINE(0x2C5, half, "VECHalfAngle():  NULL VecPtr 'half' ");
    aTmp.x = -a->x;
    aTmp.y = -a->y;
    aTmp.z = -a->z;
    bTmp.x = -b->x;
    bTmp.y = -b->y;
    bTmp.z = -b->z;
    VECNormalize(&aTmp, &aTmp);
    VECNormalize(&bTmp, &bTmp);
    VECAdd(&aTmp, &bTmp, &hTmp);
    if (VECDotProduct(&hTmp, &hTmp) > 0.0f) {
        VECNormalize(&hTmp, half);
        return;
    }
    *half = hTmp;
}

void C_VECReflect(Vec *src, Vec *normal, Vec *dst) {
    f32 cosA;
    Vec uI;
    Vec uN;

    ASSERTMSGLINE(0x2FB, src, "VECReflect():  NULL VecPtr 'src' ");
    ASSERTMSGLINE(0x2FC, normal, "VECReflect():  NULL VecPtr 'normal' ");
    ASSERTMSGLINE(0x2FD, dst, "VECReflect():  NULL VecPtr 'dst' ");

    uI.x = -src->x;
    uI.y = -src->y;
    uI.z = -src->z;
    VECNormalize(&uI, &uI);
    VECNormalize(normal, &uN);
    cosA = VECDotProduct(&uI, &uN);
    dst->x = (2.0f * uN.x * cosA) - uI.x;
    dst->y = (2.0f * uN.y * cosA) - uI.y;
    dst->z = (2.0f * uN.z * cosA) - uI.z;
    VECNormalize(dst, dst);
}

f32 C_VECSquareDistance(Vec *a, Vec *b) {
    Vec diff;

    diff.x = a->x - b->x;
    diff.y = a->y - b->y;
    diff.z = a->z - b->z;
    return (diff.z * diff.z) + ((diff.x * diff.x) + (diff.y * diff.y));
}

f32 PSVECSquareDistance(register Vec *a, register Vec *b) {
    register f32 v0yz, v1yz, v0xy, v1xy, dyz, dxy;
    register f32 sqdist;

    asm {
        psq_l v0yz, 0x4(a), 0, 0
        psq_l v1yz, 0x4(b), 0, 0
        ps_sub dyz, v0yz, v1yz
        psq_l v0xy, 0x0(a), 0, 0
        psq_l v1xy, 0x0(b), 0, 0
        ps_mul dyz, dyz, dyz
        ps_sub dxy, v0xy, v1xy
        ps_madd sqdist, dxy, dxy, dyz
        ps_sum0 sqdist, sqdist, dyz, dyz
    }

    return sqdist;
}

f32 C_VECDistance(Vec *a, Vec *b) {
    return sqrtf(C_VECSquareDistance(a, b));
}

f32 PSVECDistance(const register Vec* a, const register Vec* b) {
    register f32 v0yz, v1yz, v0xy, v1xy, dyz, dxy;
    register f32 sqdist, rdist;
    register f32 nwork0, nwork1;
    register f32 c_half, c_three, c_zero;

    asm {
        psq_l v0yz, 0x4(a), 0, 0
        psq_l v1yz, 0x4(b), 0, 0
        ps_sub dyz, v0yz, v1yz
        psq_l v0xy, 0x0(a), 0, 0
        psq_l v1xy, 0x0(b), 0, 0
        ps_mul dyz, dyz, dyz
        ps_sub dxy, v0xy, v1xy
    }

    c_half  = 0.5f;

    asm {
        ps_madd sqdist, dxy, dxy, dyz
        fsubs c_zero, c_half, c_half
        ps_sum0 sqdist, sqdist, dyz, dyz
        fcmpu cr0, c_zero, sqdist
        beq L_00000CBC
    }

    c_three = 3.0f;

    asm {
        frsqrte rdist, sqdist
        fmuls nwork0, rdist, rdist
        fmuls nwork1, rdist, c_half
        fnmsubs nwork0, nwork0, sqdist, c_three
        fmuls rdist, nwork0, nwork1
        fmuls sqdist, sqdist, rdist
    L_00000CBC:
    }

    return sqdist;
}
