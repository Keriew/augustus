#ifndef AUGUSTUS_SIMPLEX_NOISE_H
#define AUGUSTUS_SIMPLEX_NOISE_H

typedef struct {
    int perm[256];
    int permGradIndex2D[256];
} SimplexNoise;

static double extrapolate(SimplexNoise *, int xsb, int ysb, double dx, double dy);

void initSimplex(SimplexNoise *ctx, int64_t seed);

double simplex2D(SimplexNoise *ctx, double x, double y);

double simplex2D_octaves(SimplexNoise *ctx,
                         double x, double y,
                         int octaves,
                         double persistence,
                         double lacunarity);
#endif //AUGUSTUS_SIMPLEX_NOISE_H
