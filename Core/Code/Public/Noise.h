#pragma once

#include "Core.h"
#include "MathLibrary.h"

namespace Cyan
{
    float PerlinNoise(glm::dvec3 q, f64 tiling = 256)
    {
        static i32 permutation[] = { 
            151,160,137,91,90,15,
            131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
            190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
            88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
            77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
            102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
            135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
            5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
            223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
            129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
            251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
            49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
            138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
        };

        struct PermutationLUT
        {
            PermutationLUT()
                : table(512)
            {
                for (i32 i = 0; i < 512; ++i)
                {
                    table[i] = permutation[i % 256];
                }
            }

            i32 operator[](i32 index)
            {
                assert(index < 512);
                return table[index];
            }

        private:
            std::vector<i32> table;
        };

        static PermutationLUT p;

        auto fade = [](f64 t) {
            return t * t * t * (t * (t * 6. - 15.) + 10.);
        };

        // the gradient function calculates the dot product between a pseudo-random
        // gradient vector and the vector from the input coordinate to the 8
        // surrounding points in its unit cube.
        auto grad = [](i32 hash, f64 x, f64 y, f64 z) -> f64 {
            switch (hash & 0xF)
            {
            case 0x0: return  x + y;
            case 0x1: return -x + y;
            case 0x2: return  x - y;
            case 0x3: return -x - y;
            case 0x4: return  x + z;
            case 0x5: return -x + z;
            case 0x6: return  x - z;
            case 0x7: return -x - z;
            case 0x8: return  y + z;
            case 0x9: return -y + z;
            case 0xA: return  y - z;
            case 0xB: return -y - z;
            case 0xC: return  y + x;
            case 0xD: return -y + z;
            case 0xE: return  y - x;
            case 0xF: return -y - z;
            default: return 0;
            }
        };

        auto lerp = [](f64 a, f64 b, f64 t) {
            return a + t * (b - a);
        };

        // tiling
        q = glm::mod(q, tiling);
        // derive integral coordinates of the bottom left corners of the cube where current point resides in
        i32 xi = (i32)q.x & 255;
        i32 yi = (i32)q.y & 255;
        i32 zi = (i32)q.z & 255;
        // calculate the coordinates of p relative to current cube cell
        f64 xf = q.x - (i32)q.x;
        f64 yf = q.y - (i32)q.y;
        f64 zf = q.z - (i32)q.z;
        // smooth p's relative coordinates
        f64 u = fade(xf);
        f64 v = fade(yf);
        f64 w = fade(zf);
        auto inc = [tiling](i32 i) -> i32 {
            i += 1;
            i = i % (i32)tiling;
            return i;
        };
        // generate a hash value for each corner of current cube cell, 8 in total
        i32 aaa, aba, aab, abb, baa, bba, bab, bbb;
        aaa = p[p[p[xi] + yi] + zi];
        aba = p[p[p[xi] + inc(yi)] + zi];
        aab = p[p[p[xi] + yi] + inc(zi)];
        abb = p[p[p[xi] + inc(yi)] + inc(zi)];
        baa = p[p[p[inc(xi)] + yi] + zi];
        bba = p[p[p[inc(xi)] + inc(yi)] + zi];
        bab = p[p[p[inc(xi)] + yi] + inc(zi)];
        bbb = p[p[p[inc(xi)] + inc(yi)] + inc(zi)];
        // calculate the dot product of 8 random gradient vector and distance vector
        double x1, x2, y1, y2;
        x1 = lerp(grad(aaa, xf, yf, zf), grad(baa, xf - 1, yf, zf), u);
        x2 = lerp(grad(aba, xf, yf - 1, zf), grad(bba, xf - 1, yf - 1, zf), u);
        y1 = lerp(x1, x2, v);
        x1 = lerp(grad(aab, xf, yf, zf - 1), grad(bab, xf - 1, yf, zf - 1), u);
        x2 = lerp(grad(abb, xf, yf - 1, zf - 1), grad(bbb, xf - 1, yf - 1, zf - 1), u);
        y2 = lerp(x1, x2, v);
        return (f32)((lerp(y1, y2, w) + 1) / 2);
    }

    float PerlinNoise(glm::dvec3 q, f64 tiling, f64 frequency, f64 amplitude, i32 numOctaves, f64 persistence)
    {
        f64 total = 0;
        f64 maxValue = 0;
        for (i32 i = 0; i < numOctaves; ++i) 
        {
            total += PerlinNoise(q * frequency, tiling) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= 2;
        }
        return total / maxValue;
    }
}
