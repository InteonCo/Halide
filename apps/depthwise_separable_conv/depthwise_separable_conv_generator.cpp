#include "Halide.h"

namespace {

using namespace Halide;
using namespace Halide::BoundaryConditions;

class DepthwiseSeparableConvolution : public Generator<DepthwiseSeparableConvolution> {
public:
    // [in_channels, width, height, batch_size]
    Input<Buffer<float>> input_{"input", 4};

    // [channel_multiplier, in_channels, filter_width, filter_height]
    Input<Buffer<float>> depthwise_filter_{"depthwise_filter", 4};

    // [out_channels, channel_multiplier * in_channels]
    Input<Buffer<float>> pointwise_filter_{"pointwise_filter", 2};

    // [out_channels]
    Input<Buffer<float>> bias_{"bias", 1};

    Input<int> pad_width_{"pad_width"};
    Input<int> pad_height_{"pad_height"};

    // [out_channels, width, height, batch_size]
    Output<Buffer<float>> output_{"output", 4};

    void generate() {
        // The algorithm. It will be a generic depthwise convolution,
        // with no assumptions about input sizes or shapes. This makes
        // it especially challenging to schedule.

        // Some free variables, where x and y represent the spatial dimensions.
        Var x("x"), y("y"), d("d"), b("b");

        // Pad x and y with 0.

        Func input_bounded =
            constant_exterior(input_, 0.f,
                              {{Expr(), Expr()},
                               {0, input_.dim(1).extent()},
                               {0, input_.dim(2).extent()},
                               {Expr(), Expr()}});

        // Shift the input spatially in [x, y] by -[pad_width, pad_height].
        Func shifted_input_with_offset("shifted_input_with_offset");
        shifted_input_with_offset(d, x, y, b) = input_bounded(
            d, x - pad_width_, y - pad_height_, b);

        Expr channel_multiplier = depthwise_filter_.dim(0).extent();

        // Convolve the image depthwise -- for each input channel,
        // generate channel_multiplier number of intermediate channels using convolution
        Func depthwise_convolved("depthwise_convolved");
        RDom depthwise_filter_dom(0, depthwise_filter_.dim(0).extent(),
                                  0, depthwise_filter_.dim(2).extent(),
                                  0, depthwise_filter_.dim(3).extent());
        depthwise_convolved(d, x, y, b) +=
            depthwise_filter_(depthwise_filter_dom[0],
                              d,
                              depthwise_filter_dom[1],
                              depthwise_filter_dom[2]) *
            shifted_input_with_offset(
                d / channel_multiplier,
                x + depthwise_filter_dom[1],
                y + depthwise_filter_dom[2],
                b);
        // Convolve the image point-wise: for each pixel we map from
        // input_channels * channel_multiplier number of channels to output_channels
        Func pointwise_convolved("pointwise_convolved");
        RDom pointwise_filter_dom(0, pointwise_filter_.dim(1).extent());
        pointwise_convolved(d, x, y, b) = bias_(d);
        pointwise_convolved(d, x, y, b) +=
            pointwise_filter_(d, pointwise_filter_dom) *
            depthwise_convolved(pointwise_filter_dom, x, y, b);
        // ReLU
        output_(d, x, y, b) = max(pointwise_convolved(d, x, y, b), 0.f);

        // Second layer of MobileNet v2
        const int N = 4, CI = 32, CO = 16, CM = 1, W = 112, H = 112;

        // The schedule.
        if (auto_schedule) {
            input_.dim(0).set_estimate(0, CI);
            input_.dim(1).set_estimate(0, W);
            input_.dim(2).set_estimate(0, H);
            input_.dim(3).set_estimate(0, N);

            depthwise_filter_.dim(0).set_estimate(0, CI / CO);
            depthwise_filter_.dim(1).set_estimate(0, CI);
            depthwise_filter_.dim(2).set_estimate(0, 3);
            depthwise_filter_.dim(3).set_estimate(0, 3);

            pointwise_filter_.dim(0).set_estimate(0, CO);
            pointwise_filter_.dim(1).set_estimate(0, CI * CM);

            bias_.dim(0).set_estimate(0, CO);

            pad_width_.set_estimate(1);
            pad_height_.set_estimate(1);

            output_.dim(0).set_estimate(0, CO);
            output_.dim(1).set_estimate(0, W);
            output_.dim(2).set_estimate(0, H);
            output_.dim(3).set_estimate(0, N);
        } else {
            Var bi, xi, yi, di, xii, yii;

            output_
                .split(x, x, xi, 8, TailStrategy::RoundUp)
                .split(y, y, yi, 4, TailStrategy::RoundUp)
                .split(xi, xi, xii, 4, TailStrategy::RoundUp)
                .split(yi, yi, yii, 2, TailStrategy::RoundUp)
                .split(d, d, di, 16, TailStrategy::RoundUp)
                .reorder(xii, yii, di, xi, yi, d, x, y, b)
                .gpu_threads(di, xi, yi)
                .fuse(y, b, b)
                .gpu_blocks(d, x, b)
                .unroll(xii)
                .unroll(yii);

            depthwise_convolved.in()
                .compute_root()
                .split(x, x, xi, 4, TailStrategy::RoundUp)
                .split(y, y, yi, 4, TailStrategy::RoundUp)
                .split(xi, xi, xii, 2, TailStrategy::RoundUp)
                .split(yi, yi, yii, 2, TailStrategy::RoundUp)
                .split(d, d, di, 32, TailStrategy::RoundUp)
                .reorder(xii, yii, di, xi, yi, d, x, y, b)
                .gpu_threads(di, xi, yi)
                .fuse(y, b, b)
                .gpu_blocks(d, x, b)
                .unroll(xii)
                .unroll(yii);

            RVar rxi, ryi, rxo, ryo;
            depthwise_convolved
                .compute_at(depthwise_convolved.in(), di)
                .unroll(x)
                .unroll(y)
                .update()
                .reorder(x, y, depthwise_filter_dom[1], depthwise_filter_dom[2], depthwise_filter_dom[0])
                .unroll(x)
                .unroll(y);

            pointwise_convolved.compute_at(output_, di)
                .reorder_storage(x, d, y)
                .reorder(x, y, d)
                .unroll(x)
                .unroll(y)
                .update()
                .reorder(x, y, pointwise_filter_dom, d)
                .unroll(x)
                .unroll(y);
        }
    }
};

}  // namespace

HALIDE_REGISTER_GENERATOR(DepthwiseSeparableConvolution, depthwise_separable_conv)
