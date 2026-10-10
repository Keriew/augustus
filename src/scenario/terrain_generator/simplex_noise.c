#include <stdint.h>
#include "simplex_noise.h"
#define STRETCH_CONSTANT_2D (-0.211324865405187) // (1/sqrt(2+1)-1)/2
#define SQUISH_CONSTANT_2D  (0.366025403784439)  // (sqrt(2+1)-1)/2

#define NORM_CONSTANT_2D (47.0)

static const int gradients2D[] = {
    5, 2, 2, 5,
    -5, 2, -2, 5,
    5, -2, 2, -5,
    -5, -2, -2, -5,
};

static int fastFloor(double x) {
    int xi = (int) x;
    return x < xi ? xi - 1 : xi;
}

static double extrapolate(SimplexNoise *ctx, int xsb, int ysb, double dx, double dy) {
    int index = ctx->permGradIndex2D[(ctx->perm[(xsb & 0xFF)] + ysb) & 0xFF];
    return gradients2D[index] * dx + gradients2D[index + 1] * dy;
}

void initSimplex(SimplexNoise *ctx, int64_t seed) {
    int source[256];
    for (int i = 0; i < 256; i++)
        source[i] = i;

    for (int i = 255; i >= 0; i--) {
        seed = seed * 6364136223846793005LL + 1442695040888963407LL;
        int r = (int) ((seed + 31) % (i + 1));
        if (r < 0) r += (i + 1);

        ctx->perm[i] = source[r];
        ctx->permGradIndex2D[i] = (ctx->perm[i] % (sizeof(gradients2D) / sizeof(int) / 2)) * 2;
        source[r] = source[i];
    }
}

double simplex2D(SimplexNoise *ctx, double x, double y) {
    double stretchOffset = (x + y) * STRETCH_CONSTANT_2D;
    double xs = x + stretchOffset;
    double ys = y + stretchOffset;

    int xsb = fastFloor(xs);
    int ysb = fastFloor(ys);

    double squishOffset = (xsb + ysb) * SQUISH_CONSTANT_2D;
    double dx0 = x - (xsb + squishOffset);
    double dy0 = y - (ysb + squishOffset);

    double value = 0;

    // Contribution (0,0)
    double attn0 = 2 - dx0 * dx0 - dy0 * dy0;
    if (attn0 > 0) {
        attn0 *= attn0;
        value += attn0 * attn0 * extrapolate(ctx, xsb, ysb, dx0, dy0);
    }

    // Contribution (1,0)
    double dx1 = dx0 - 1 - SQUISH_CONSTANT_2D;
    double dy1 = dy0 - 0 - SQUISH_CONSTANT_2D;
    double attn1 = 2 - dx1 * dx1 - dy1 * dy1;
    if (attn1 > 0) {
        attn1 *= attn1;
        value += attn1 * attn1 * extrapolate(ctx, xsb + 1, ysb, dx1, dy1);
    }

    // Contribution (0,1)
    double dx2 = dx0 - 0 - SQUISH_CONSTANT_2D;
    double dy2 = dy0 - 1 - SQUISH_CONSTANT_2D;
    double attn2 = 2 - dx2 * dx2 - dy2 * dy2;
    if (attn2 > 0) {
        attn2 *= attn2;
        value += attn2 * attn2 * extrapolate(ctx, xsb, ysb + 1, dx2, dy2);
    }

    return value / NORM_CONSTANT_2D;
}

double simplex2D_octaves(SimplexNoise *ctx,
                         double x, double y,
                         int octaves,
                         double persistence,
                         double lacunarity) {
    double amplitude = 1.0;
    double frequency = 1.0;
    double value = 0.0;
    double maxAmplitude = 0.0; // for normalization

    for (int i = 0; i < octaves; i++) {
        value += simplex2D(ctx, x * frequency, y * frequency) * amplitude;
        maxAmplitude += amplitude;

        amplitude *= persistence;
        frequency *= lacunarity;
    }

    // Normalize to roughly [-1, 1]
    return value / maxAmplitude;
}
