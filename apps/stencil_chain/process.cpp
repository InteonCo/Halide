#include <chrono>
#include <cstdio>

#include "stencil_chain.h"
#ifndef NO_AUTO_SCHEDULE
#include "stencil_chain_auto_schedule.h"
#include "stencil_chain_gradient_auto_schedule.h"
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
    Buffer<uint16_t> input = load_and_convert_image(argv[1]);
    // Just take the red channel
    input.slice(2, 0);

    Buffer<uint16_t> output(input.width(), input.height());

    multi_way_bench({
        {"Manual", [&]() { stencil_chain(input, output); output.device_sync(); }},
    #ifndef NO_AUTO_SCHEDULE
        {"Auto-scheduled", [&]() { stencil_chain_auto_schedule(input, output); output.device_sync(); }},
        {"Gradient auto-scheduled", [&]() { stencil_chain_gradient_auto_schedule(input, output); output.device_sync();}}
    #endif
        }
    );

    convert_and_save_image(output, argv[3]);

    printf("Success!\n");
    return 0;
}
