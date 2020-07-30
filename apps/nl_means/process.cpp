#include <chrono>
#include <cstdio>

#include "nl_means.h"
#ifndef NO_AUTO_SCHEDULE
#include "nl_means_auto_schedule.h"
#include "nl_means_gradient_auto_schedule.h"
#endif

#include "HalideBuffer.h"
#include "benchmark_util.h"
#include "halide_benchmark.h"
#include "halide_image_io.h"

using namespace Halide::Runtime;
using namespace Halide::Tools;

int main(int argc, char **argv) {
    if (argc < 7) {
        printf("Usage: ./process input.png patch_size search_area sigma timing_iterations output.png\n"
               "e.g.: ./process input.png 7 7 0.12 10 output.png\n");
        return 0;
    }

    Buffer<float> input = load_and_convert_image(argv[1]);
    int patch_size = atoi(argv[2]);
    int search_area = atoi(argv[3]);
    float sigma = atof(argv[4]);
    Buffer<float> output(input.width(), input.height(), 3);

    printf("Input size: %d by %d, patch size: %d, search area: %d, sigma: %f\n",
           input.width(), input.height(), patch_size, search_area, sigma);

    multi_way_bench({{"nl_means Manual", [&]() { nl_means(input, patch_size, search_area, sigma, output); output.device_sync(); }},
#ifndef NO_AUTO_SCHEDULE
                     {"nl_means Auto-scheduled", [&]() { nl_means_auto_schedule(input, patch_size, search_area, sigma, output); output.device_sync(); }},
                     {"nl_means Gradient auto-scheduled", [&]() { nl_means_gradient_auto_schedule(input, patch_size, search_area, sigma, output); output.device_sync(); }}
#endif
    });

    convert_and_save_image(output, argv[6]);

    printf("Success!\n");
    return 0;
}
