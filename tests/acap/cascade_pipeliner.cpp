/* Define a pipeliner to execute some functions distributed along
   the cascade stream in a dataflow way

   RUN: %{execute}%s
*/

#include <CL/sycl.hpp>

#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/hana.hpp>
#include <range/v3/all.hpp>

using namespace boost;
using namespace ranges;
using namespace cl::sycl;
using namespace cl::sycl::vendor::xilinx;
using namespace cl::sycl::vendor::xilinx::acap::aie;

/// Used to debug metaprogramming on type
template <typename T>
class display_type;
/// Used to debug metaprogramming on value
template <auto V>
class display_value;

// An executor to execute stage_number first stages of a pipeline
template <int stage_number>
auto pipeline_executor = [] (auto input, auto stages) {
  if constexpr (stage_number == 0)
    return input;
  else
    // Apply requested stage on the rest of the pipeline
    return hana::at_c<stage_number - 1>(stages)
      (pipeline_executor<stage_number - 1>(input, stages));
};

/// Construct an execution pipeline on the host
auto host_pipeliner = [] (auto... stages) {
  auto s = hana::make_tuple(stages...);
  return [=] (auto input) {
           return pipeline_executor<sizeof...(stages)>(input, s);
         };
};

/// A generic AIE program instantiating a pipeline executor
template <typename FirstT, typename Stages>
struct cascade_executor {
  static constexpr Stages s;
  static auto constexpr stage_number = hana::value(hana::length(s));
  static auto constexpr last_stage = stage_number - 1;

  /** Compute the type of the output at stage i

      0 it the input type, 1 for the output type of the first stage, 2
      for the output type of the second stage, etc. */
  template <int i>
  using StageOutput = decltype(pipeline_executor<i>(std::declval<FirstT>(), s));
  using LastT = StageOutput<stage_number>;

  template <typename AIE, int X, int Y>
  struct tile_program : acap::aie::tile<AIE, X, Y> {
    // Get tile information through this shortcut
    using t = acap::aie::tile<AIE, X, Y>;

    void run() {
      // Do something only for the cores concerned by the pipeline
      if constexpr (t::cascade_linear_id() <= last_stage) {
        using InputT = StageOutput<t::cascade_linear_id()>;
        using OutputT = StageOutput<t::cascade_linear_id() + 1>;
        InputT input;
        if constexpr (t::is_cascade_start())
          // Read from input 0 of the AXI stream switch
          input = t::template in<InputT>(0).read();
        else
          // Normal stage: read the input from the cascade neighbor
          input = t::template get_cascade_stream_in<InputT>().read();
        std::cout << "< Tile(" << X << ',' << Y << ") is reading "
                  << input << std::endl;
        // The computation for this pipeline stage
        OutputT output = hana::at_c<t::cascade_linear_id()>(s)(input);
        std::cout << "> Tile(" << X << ',' << Y << ") is writing "
                  << output << std::endl;
        if constexpr (t::cascade_linear_id() ==  last_stage)
          // Last stage: ship the result through the AIE NoC
          t::template out<OutputT>(0) << output;
        else
          // Normal stage: send the result to the cascade neighbor
          t::template get_cascade_stream_out<OutputT>() << output;
      }
    }
  };

  acap::aie::array<acap::aie::layout::size<3, 2>, tile_program> a;

  auto get_executor() {
    // NoC connection between shim and input of the pipeline
    a.template connect<FirstT>(port::shim { 0, 0 }, port::tile { 0, 0, 0 });
    // NoC connection between output of the pipeline and the shim
    auto last_x = decltype(a)::geo::cascade_linear_x(last_stage);
    auto last_y = decltype(a)::geo::cascade_linear_y(last_stage);
    a.template connect<LastT>(port::tile { last_x, last_y, 0 },
                              port::shim { 0, 0 });
    return [&] (FirstT input) {
             a.shim(0).template bli_out<0, FirstT>() << input;
             a.run();
             return a.shim(0).template bli_in<0, LastT>().read();
           };
  }
};

auto make_cascade_pipeliner = [] (auto input, auto... stages) {
  auto s = hana::make_tuple(stages...);
  return cascade_executor<decltype(input), decltype(s)> {};
};

int main() {
  /// Some pipeline stages
  auto p1 = [] (auto input) { return input + 3; };
  auto p2 = [] (auto input) { return input*7; };
  auto p3 = [] (auto input) { return input*input; };
  auto p4 = [] (auto input) { return double(input)/42; };

  auto hp = host_pipeliner(p1, p2, p3, p4);

  ranges::for_each(view::ints(0, 10) | view::transform(hp),
                   [] (auto x) { std::cout << x << std::endl; });

  auto aie_cp = make_cascade_pipeliner(0, p1, p2, p3, p4);
  auto cp = aie_cp.get_executor();

  ranges::for_each(view::ints(0, 10) | view::transform(cp),
                   [] (auto x) { std::cout << x << std::endl; });
}