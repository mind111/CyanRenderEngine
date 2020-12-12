#pragma once

float* buildPlaneMesh(int gridSize) {
    int lineIdx = 0;
    int numOfLines = gridSize + 1;
    float cellSize = 1.f / gridSize;
    float startX = -.5f;
    float startZ = -startX;
    // 6 extra verts for drawing three axis
    int numOfVerts = 4 * (gridSize + 1) + 6;
    float* verts = new float[numOfVerts * 3 * 2]; 
    int vertIdx = lineIdx * 2 * 2;
    for (; lineIdx < numOfLines; lineIdx++) {
        // vertical line
        verts[vertIdx * 6    ] = startX + lineIdx * cellSize;    // x
        verts[vertIdx * 6 + 1] = 0.f;                            // y
        verts[vertIdx * 6 + 2] = startZ;                         // z
        verts[vertIdx * 6 + 3] = .5f;                       // r
        verts[vertIdx * 6 + 4] = .5f;                       // g
        verts[vertIdx * 6 + 5] = .5f;                       // b
        vertIdx++;

        verts[vertIdx * 6    ] =  startX + lineIdx * cellSize;   // x
        verts[vertIdx * 6 + 1] = 0.f;                            // y
        verts[vertIdx * 6 + 2] = -startZ;                        // z
        verts[vertIdx * 6 + 3] = .5f;                       // r
        verts[vertIdx * 6 + 4] = .5f;                       // g
        verts[vertIdx * 6 + 5] = .5f;                       // b
        vertIdx++;

        // horizontal li6e
        verts[vertIdx * 6    ] = startX;                         // x
        verts[vertIdx * 6 + 1] = 0.f;                            // y
        verts[vertIdx * 6 + 2] = startZ- lineIdx * cellSize;     // z
        verts[vertIdx * 6 + 3] = .5f;                       // r
        verts[vertIdx * 6 + 4] = .5f;                       // g
        verts[vertIdx * 6 + 5] = .5f;                       // b
        vertIdx++;

        verts[vertIdx * 6    ] = -startX;                        // x
        verts[vertIdx * 6 + 1] = 0.f;                            // y
        verts[vertIdx * 6 + 2] =  startZ - lineIdx * cellSize;   // z
        verts[vertIdx * 6 + 3] = .5f;                       // r
        verts[vertIdx * 6 + 4] = .5f;                       // g
        verts[vertIdx * 6 + 5] = .5f;                       // b
        vertIdx++;
    }

    // verts for three axis
    verts[vertIdx * 6    ] = -1.f;                      // x
    verts[vertIdx * 6 + 1] = 0.f;                       // y
    verts[vertIdx * 6 + 2] = 0.f;                       // z
    verts[vertIdx * 6 + 3] = 1.f;                       // r
    verts[vertIdx * 6 + 4] = 0.f;                       // g
    verts[vertIdx * 6 + 5] = 0.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 1.f;                       // x
    verts[vertIdx * 6 + 1] = 0.f;                       // y
    verts[vertIdx * 6 + 2] = 0.f;                       // z
    verts[vertIdx * 6 + 3] = 1.f;                       // r
    verts[vertIdx * 6 + 4] = 0.f;                       // g
    verts[vertIdx * 6 + 5] = 0.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 0.f;                       // x
    verts[vertIdx * 6 + 1] = -1.f;                      // y
    verts[vertIdx * 6 + 2] = 0.f;                       // z
    verts[vertIdx * 6 + 3] = 0.f;                       // r
    verts[vertIdx * 6 + 4] = 1.f;                       // g
    verts[vertIdx * 6 + 5] = 0.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 0.f;                       // x
    verts[vertIdx * 6 + 1] = 1.f;                       // y
    verts[vertIdx * 6 + 2] = 0.f;                       // z
    verts[vertIdx * 6 + 3] = 0.f;                       // r
    verts[vertIdx * 6 + 4] = 1.f;                       // g
    verts[vertIdx * 6 + 5] = 0.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 0.f;                       // x
    verts[vertIdx * 6 + 1] = 0.f;                       // y
    verts[vertIdx * 6 + 2] = -1.f;                      // z
    verts[vertIdx * 6 + 3] = 0.f;                       // r
    verts[vertIdx * 6 + 4] = 0.f;                       // g
    verts[vertIdx * 6 + 5] = 1.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 0.f;                       // x
    verts[vertIdx * 6 + 1] = 0.f;                       // y
    verts[vertIdx * 6 + 2] = 1.f;                       // z
    verts[vertIdx * 6 + 3] = 0.f;                       // r
    verts[vertIdx * 6 + 4] = 0.f;                       // g
    verts[vertIdx * 6 + 5] = 1.f;                       // b
    vertIdx++;
    return verts;
}