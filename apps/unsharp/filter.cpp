#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "HalideRuntime.h"
#include "HalideBuffer.h"

#include "unsharp.h"
#ifndef NO_AUTO_SCHEDULE
#include "unsharp_auto_schedule.h"
#include "unsharp_gradient_auto_schedule.h"
#endif

#include "benchmark_util.h"
#include "halide_image_io.h"

using namespace Halide::Tools;

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s in out\n", argv[0]);
        return 1;
    }

    Halide::Runtime::Buffer<float> input = load_and_convert_image(argv[1]);
    Halide::Runtime::Buffer<float> output(input.width(), input.height(), 3);

    multi_way_bench({
        {"Manual", [&]() { unsharp(input, output); output.device_sync(); }},
    #ifndef NO_AUTO_SCHEDULE
        {"Auto-scheduled", [&]() { unsharp_auto_schedule(input, output); output.device_sync(); }},
        {"Gradient auto-scheduled", [&]() {unsharp_gradient_auto_schedule(input, output); output.device_sync(); }}
    #endif
    });

    convert_and_save_image(output, argv[2]);

    printf("Success!\n");
    return 0;
}
