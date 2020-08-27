#include <chrono>
#include <cstdio>

#include "ahd_demosaic.h"
#ifndef NO_AUTO_SCHEDULE
#include "ahd_demosaic_gradient_auto_schedule.h"
#endif

#include "benchmark_util.h"
#include "HalideBuffer.h"
#include "halide_benchmark.h"
#include "halide_image_io.h"

using namespace Halide::Runtime;
using namespace Halide::Tools;

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: ./process input.png timing_iterations output.png\n"
               "e.g.: ./process input.png 10 output.png\n");
        return 0;
    }

    // Input may be a PNG8
    float image[32][128][128][1];
    float g_convex[1][5][5][16];
    float g_filter[1][5][5][16];
    float chroma_v[1][5][5][2];
    float chroma_h[1][5][5][2];
    float chroma_q[1][5][5][2];
    Buffer<float>input(image);
    Buffer<float> g_convex_weights(g_convex);
    Buffer<float> g_filter_weights(g_filter);
    Buffer<float> chroma_v_weights(chroma_v);
    Buffer<float> chroma_q_weights(chroma_q);
    Buffer<float> chroma_h_weights(chroma_h);

    Buffer<float> output(3, 128, 128, 32);

    multi_way_bench({
        {"ahd_demosaic Manual", [&]() { ahd_demosaic(input, g_convex_weights, g_filter_weights, chroma_v_weights, chroma_q_weights, chroma_h_weights, output); output.device_sync(); }},
    #ifndef NO_AUTO_SCHEDULE
        //{"ahd_demosaic Auto-scheduled", [&]() { ahd_demosaic_auto_schedule(input, output); output.device_sync(); }},
        {"ahd_demosaic Gradient auto-scheduled", [&]() { ahd_demosaic_gradient_auto_schedule(input, g_convex_weights, g_filter_weights, chroma_v_weights, chroma_q_weights, chroma_h_weights, output); output.device_sync();}}
    #endif
        }
    );

    printf("Success!\n");
    return 0;
}
