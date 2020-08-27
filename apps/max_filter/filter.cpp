#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "HalideRuntime.h"
#include "HalideBuffer.h"

#include "max_filter.h"
#ifndef NO_AUTO_SCHEDULE
    #include "max_filter_auto_schedule.h"
#endif
#ifndef NO_GRADIENT_AUTO_SCHEDULE
    #include "max_filter_gradient_auto_schedule.h"
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
        {"max_filter Manual", [&]() { max_filter(input, output); output.device_sync(); }},
    #ifndef NO_AUTO_SCHEDULE
        {"max_filter Auto-scheduler", [&]() { max_filter_auto_schedule(input, output); output.device_sync(); }},
    #endif
    #ifndef NO_GRADIENT_AUTO_SCHEDULE
        {"max_filter Gradient auto-scheduler", [&]() { max_filter_gradient_auto_schedule(input, output); output.device_sync();}}
    #endif
        });

    output.copy_to_host();

    convert_and_save_image(output, argv[2]);

    printf("Success!\n");
    return 0;
}
