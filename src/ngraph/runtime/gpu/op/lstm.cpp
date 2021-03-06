//*****************************************************************************
// Copyright 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#include "ngraph/runtime/gpu/op/lstm.hpp"
#include "ngraph/log.hpp"
#include "ngraph/util.hpp"

using namespace std;
using namespace ngraph;

shared_ptr<Node> op::gpu::Lstm::copy_with_new_args(const NodeVector& new_args) const
{
    if (!m_fused_inputs)
    {
        NGRAPH_ASSERT(new_args.size() == 7) << "Incorrect number of new arguments";

        return make_shared<Lstm>(new_args.at(0),
                                 new_args.at(1),
                                 new_args.at(2),
                                 new_args.at(3),
                                 new_args.at(4),
                                 new_args.at(5),
                                 new_args.at(6));
    }
    else
    {
        NGRAPH_ASSERT(new_args.size() == 5 && m_fused_inputs)
            << "Incorrect number of new arguments";

        return make_shared<Lstm>(
            new_args.at(0), new_args.at(1), new_args.at(2), new_args.at(3), new_args.at(4));
    }
}

op::gpu::Lstm::Lstm(std::shared_ptr<Node> input_xt_1,
                    std::shared_ptr<Node> i2h_weights,
                    std::shared_ptr<Node> hidden_state_ht_1,
                    std::shared_ptr<Node> h2h_weights,
                    std::shared_ptr<Node> i2h_bias,
                    std::shared_ptr<Node> h2h_bias,
                    std::shared_ptr<Node> cell_state_ct_1)
    : Op("Lstm",
         {input_xt_1,
          i2h_weights,
          hidden_state_ht_1,
          h2h_weights,
          i2h_bias,
          h2h_bias,
          cell_state_ct_1})
    , m_output_tensor_shape(hidden_state_ht_1->get_shape())
    , m_output_cell_shape(cell_state_ct_1->get_shape())
    , m_num_timesteps(1)
    , m_num_gates_per_cell(4)
    , m_src_sequence_length(1)
    , m_src_layer_feature_size(static_cast<int>(input_xt_1->get_shape()[1]))
    , m_src_iter_feature_size(static_cast<int>(hidden_state_ht_1->get_shape()[1]))
    , m_num_cell_states(2)
    , m_direction(1)
    , m_num_fused_layers(1)
    , m_fused_inputs(false)
{
    NGRAPH_ASSERT(input_xt_1->get_shape().size() == i2h_weights->get_shape().size())
        << "input_xt_1 and i2h weights size dont match";
    NGRAPH_ASSERT(hidden_state_ht_1->get_shape().size() == h2h_weights->get_shape().size())
        << "hidden_state_ht_1 and h2h weights size dont match";
    NGRAPH_ASSERT(input_xt_1->get_shape().size() == 2) << "input_xt_1 doesnt have a rank 2";

    m_batch_size = static_cast<int>(input_xt_1->get_shape()[0]);

    NGRAPH_ASSERT(shape_size(input_xt_1->get_shape()) ==
                  m_src_sequence_length * m_batch_size * m_src_layer_feature_size)
        << "input_xt_1 size is not equal t*n*c";
    NGRAPH_ASSERT(i2h_bias->get_shape()[0] == i2h_weights->get_shape()[0] &&
                  h2h_bias->get_shape()[0] == h2h_weights->get_shape()[0])
        << "bias and weights_shape are not compatible";

    NGRAPH_ASSERT(m_output_tensor_shape == m_output_cell_shape)
        << "shape of recurrent input and cell state are not the same";

    auto et = input_xt_1->get_element_type();
    for (auto& lstm_input : get_arguments())
    {
        if (lstm_input->get_element_type() != et)
        {
            throw ngraph_error("all rnn inputs must have the same element type");
        }
    }

    set_output_size(2);
    set_output_type(0, hidden_state_ht_1->get_element_type(), hidden_state_ht_1->get_shape());
    set_output_type(1, cell_state_ct_1->get_element_type(), cell_state_ct_1->get_shape());
}

op::gpu::Lstm::Lstm(std::shared_ptr<Node> src_layer,
                    std::shared_ptr<Node> src_iter,
                    std::shared_ptr<Node> weights_layer,
                    std::shared_ptr<Node> weights_iter,
                    std::shared_ptr<Node> bias)
    : Op("Lstm", {src_layer, src_iter, weights_layer, weights_iter, bias})
    , m_output_tensor_shape(src_layer->get_shape())
    , m_output_cell_shape(src_iter->get_shape())
    , m_num_timesteps(1)
    , m_num_gates_per_cell(4)
    , m_src_sequence_length(1)
    , m_src_layer_feature_size(static_cast<int>(src_layer->get_shape()[1]))
    , m_src_iter_feature_size(static_cast<int>(src_iter->get_shape()[1]))
    , m_num_cell_states(2)
    , m_direction(1)
    , m_num_fused_layers(1)
    , m_fused_inputs(true)
{
    NGRAPH_ASSERT(src_layer->get_shape().size() == weights_layer->get_shape().size())
        << "src_layer and i2h weights size dont match";
    NGRAPH_ASSERT(src_iter->get_shape().size() == weights_iter->get_shape().size())
        << "src_iter and h2h weights size dont match";
    NGRAPH_ASSERT(src_layer->get_shape().size() == 2) << "src_layer doesnt have a rank 2";

    m_batch_size = static_cast<int>(src_layer->get_shape()[0] / m_num_timesteps);

    NGRAPH_ASSERT(shape_size(src_layer->get_shape()) ==
                  m_src_sequence_length * m_batch_size * m_src_layer_feature_size)
        << "src_layer size is not equal t*n*c";
    NGRAPH_ASSERT(bias->get_shape()[0] == weights_layer->get_shape()[0] &&
                  bias->get_shape()[0] == weights_iter->get_shape()[0])
        << "bias and weights_shape are not compatible";

    auto et = src_layer->get_element_type();
    for (auto& rnn_input : get_arguments())
    {
        if (rnn_input->get_element_type() != et)
        {
            throw ngraph_error("all rnn inputs must have the same element type");
        }
    }

    set_output_size(2);
    set_output_type(0,
                    src_layer->get_element_type(),
                    Shape{static_cast<unsigned long>(m_num_timesteps * m_batch_size),
                          static_cast<unsigned long>(m_src_iter_feature_size)});
    set_output_type(1,
                    src_layer->get_element_type(),
                    Shape{static_cast<unsigned long>(m_num_cell_states * m_batch_size),
                          static_cast<unsigned long>(m_src_iter_feature_size)});
}
