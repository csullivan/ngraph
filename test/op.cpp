// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// ----------------------------------------------------------------------------

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "ngraph/ngraph.hpp"

using namespace std;
using namespace ngraph;

TEST(op, is_op)
{
    auto arg0 = op::parameter(element::Float32::element_type(), {1});
    ASSERT_NE(nullptr, arg0);
    EXPECT_TRUE(arg0->is_parameter());
    EXPECT_FALSE(arg0->is_op());
}

TEST(op, is_parameter)
{
    auto arg0 = op::parameter(element::Float32::element_type(), {1});
    ASSERT_NE(nullptr, arg0);
    auto t0 = op::add(arg0, arg0);
    ASSERT_NE(nullptr, t0);
    EXPECT_FALSE(t0->is_parameter());
    EXPECT_TRUE(t0->is_op());
}