#version 450

#define NUM_THREAD_X 16
#define NUM_THREAD_Y 16
#define NUM_THREAD_Z 1
#define RGB_TO_LUM vec3(0.2125, 0.7154, 0.0721)

// local work group size  16 x 16 pixels
layout (local_size_x = NUM_THREAD_X, local_size_y = NUM_THREAD_Y, local_size_z = NUM_THREAD_Z) in;

uniform sampler2D hdrColorBuffer;

layout (binding = 0) buffer globalHistogram
{
    uint histogram[NUM_THREAD_X * NUM_THREAD_Y];
} histogramBuffer;

shared uint sharedHistogram[NUM_THREAD_X * NUM_THREAD_Y];

float rgbToLum(vec3 color)
{
    return dot(color, RGB_TO_LUM);
}

uint colorToBin(vec3 color, float minLum, float maxLum)
{
    float lum = rgbToLum(color);
    if (lum < 0.001f)
    {
        // 0th bin
        return 0;
    }
    float logLum = (log2(lum) - log2(minLum)) / (log2(maxLum) - log2(minLum));
    // plus one as the 0th bin is saved for lum < 0.001f
    return uint(logLum * 254.f + 1.f); 
}

// reference https://bruop.github.io/exposure/
/* 
    Notes:
        * This step generates a histogram of all the luminance level from 0 to 255, with each level corresponds to a integral luminance value. Each luminance value
        has a bin in the histogram. Each bin stores the frequency of that luminance level among all the pixels in a given frame. This statistical informaltion
        will be helpful later when determining the exposure of this frame.
*/
void main()
{
    sharedHistogram[gl_LocalInvocationIndex] = 0;
    barrier();

    uint totalNumThreads_x = gl_NumWorkGroups.x * NUM_THREAD_X;
    uint totalNumThreads_y = gl_NumWorkGroups.y * NUM_THREAD_Y;
    // get pixel coordinates
    vec2 uv = vec2(gl_GlobalInvocationID.x / totalNumThreads_x, gl_GlobalInvocationID.y / totalNumThreads_y);
    vec3 color = textureLod(hdrColorBuffer, uv, 0).rgb;
    uint bin = colorToBin(color, -2.f, 10.f);
    atomicAdd(sharedHistogram[bin], 1);
    // make sure all the threads among local workgroup has finish accumulating their results
    // into according bins 
    barrier();

    // add values in local histogram back into global histogram
    atomicAdd(histogramBuffer.histogram[bin], sharedHistogram[bin]);
}