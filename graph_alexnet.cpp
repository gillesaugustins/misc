/*
 * Copyright (c) 2017-2018 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "arm_compute/graph/Graph.h"
#include "arm_compute/graph/Nodes.h"
#include "support/ToolchainSupport.h"
#include "utils/GraphUtils.h"
#include "utils/Utils.h"
#include "streamline_annotate.h"

#include <cstdlib>
#include <iostream>
#include <memory>

using namespace arm_compute::utils;
using namespace arm_compute::graph;
using namespace arm_compute::graph_utils;

/** Example demonstrating how to implement AlexNet's network using the Compute Library's graph API
 *
 * @param[in] argc Number of arguments
 * @param[in] argv Arguments ( [optional] Target (0 = NEON, 1 = OpenCL, 2 = OpenCL with Tuner), [optional] Path to the weights folder, [optional] image, [optional] labels )
 */
class GraphAlexnetExample : public Example
{
public:
    void do_setup(int argc, char **argv) override
    {
        std::string data_path; /* Path to the trainable data */
        std::string image;     /* Image data */
        std::string label;     /* Label data */

        // Create a preprocessor object
        const std::array<float, 3> mean_rgb{ { 122.68f, 116.67f, 104.01f } };
        std::unique_ptr<IPreprocessor> preprocessor = arm_compute::support::cpp14::make_unique<CaffePreproccessor>(mean_rgb);

        // Set target. 0 (NEON), 1 (OpenCL), 2 (OpenCL with Tuner). By default it is NEON
        const int  int_target_hint = argc > 1 ? std::strtol(argv[1], nullptr, 10) : 0;
        TargetHint target_hint     = set_target_hint(int_target_hint);

        const bool            is_gemm_convolution5x5 = Graph::gpu_target() == arm_compute::GPUTarget::MIDGARD || target_hint == TargetHint::NEON;
        ConvolutionMethodHint convolution_5x5_hint   = is_gemm_convolution5x5 ? ConvolutionMethodHint::GEMM : ConvolutionMethodHint::DIRECT;

        // Parse arguments
        if(argc < 2)
        {
            // Print help
            std::cout << "Usage: " << argv[0] << " [target] [path_to_data] [image] [labels]\n\n";
            std::cout << "No data folder provided: using random values\n\n";
        }
        else if(argc == 2)
        {
            std::cout << "Usage: " << argv[0] << " " << argv[1] << " [path_to_data] [image] [labels]\n\n";
            std::cout << "No data folder provided: using random values\n\n";
        }
        else if(argc == 3)
        {
            data_path = argv[2];
            std::cout << "Usage: " << argv[0] << " " << argv[1] << " " << argv[2] << " [image] [labels]\n\n";
            std::cout << "No image provided: using random values\n\n";
        }
        else if(argc == 4)
        {
            data_path = argv[2];
            image     = argv[3];
            std::cout << "Usage: " << argv[0] << " " << argv[1] << " " << argv[2] << " " << argv[3] << " [labels]\n\n";
            std::cout << "No text file with labels provided: skipping output accessor\n\n";
        }
        else
        {
            data_path = argv[2];
            image     = argv[3];
            label     = argv[4];
        }

        graph << target_hint
              << Tensor(TensorInfo(TensorShape(227U, 227U, 3U, 1U), 1, DataType::F32),
                        get_input_accessor(image, std::move(preprocessor)))
              // Layer 1
              << ConvolutionLayer(
                  11U, 11U, 96U,
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv1_w.npy"),
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv1_b.npy"),
                  PadStrideInfo(4, 4, 0, 0))
              << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::RELU))
              << NormalizationLayer(NormalizationLayerInfo(NormType::CROSS_MAP, 5, 0.0001f, 0.75f))
              << PoolingLayer(PoolingLayerInfo(PoolingType::MAX, 3, PadStrideInfo(2, 2, 0, 0)))
              // Layer 2
              << convolution_5x5_hint
              << ConvolutionLayer(
                  5U, 5U, 256U,
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv2_w.npy"),
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv2_b.npy"),
                  PadStrideInfo(1, 1, 2, 2), 2)
              << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::RELU))
              << NormalizationLayer(NormalizationLayerInfo(NormType::CROSS_MAP, 5, 0.0001f, 0.75f))
              << PoolingLayer(PoolingLayerInfo(PoolingType::MAX, 3, PadStrideInfo(2, 2, 0, 0)))
              << ConvolutionMethodHint::GEMM
              // Layer 3
              << ConvolutionLayer(
                  3U, 3U, 384U,
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv3_w.npy"),
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv3_b.npy"),
                  PadStrideInfo(1, 1, 1, 1))
              << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::RELU))
              // Layer 4
              << ConvolutionLayer(
                  3U, 3U, 384U,
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv4_w.npy"),
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv4_b.npy"),
                  PadStrideInfo(1, 1, 1, 1), 2)
              << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::RELU))
              // Layer 5
              << ConvolutionLayer(
                  3U, 3U, 256U,
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv5_w.npy"),
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/conv5_b.npy"),
                  PadStrideInfo(1, 1, 1, 1), 2)
              << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::RELU))
              << PoolingLayer(PoolingLayerInfo(PoolingType::MAX, 3, PadStrideInfo(2, 2, 0, 0)))
              // Layer 6
              << FullyConnectedLayer(
                  4096U,
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/fc6_w.npy"),
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/fc6_b.npy"))
              << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::RELU))
              // Layer 7
              << FullyConnectedLayer(
                  4096U,
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/fc7_w.npy"),
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/fc7_b.npy"))
              << ActivationLayer(ActivationLayerInfo(ActivationLayerInfo::ActivationFunction::RELU))
              // Layer 8
              << FullyConnectedLayer(
                  1000U,
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/fc8_w.npy"),
                  get_weights_accessor(data_path, "/cnn_data/alexnet_model/fc8_b.npy"))
              // Softmax
              << SoftmaxLayer()
              << Tensor(get_output_accessor(label, 5));

        // In order to enable the OpenCL tuner, graph_init() has to be called only when all nodes have been instantiated
        graph.graph_init(int_target_hint == 2);
        
        ANNOTATE_CHANNEL_END(1);
        
    }
    void do_run() override
    {
        ANNOTATE_CHANNEL_COLOR(2, ANNOTATE_BLUE, "do_run");

        // Run graph
        graph.run();

        ANNOTATE_CHANNEL_END(2);

    }

private:
    Graph graph{};
};

/** Main program for AlexNet
 *
 * @param[in] argc Number of arguments
 * @param[in] argv Arguments ( [optional] Target (0 = NEON, 1 = OpenCL), [optional] Path to the weights folder, [optional] image, [optional] labels )
 */
int main(int argc, char **argv)
{
    int st;

    ANNOTATE_SETUP;

    st= arm_compute::utils::run_example<GraphAlexnetExample>(argc, argv);

    ANNOTATE_MARKER_STR("main complete");

    return (st);
}
