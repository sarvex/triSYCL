/* Testing the cascade stream

   RUN: %{execute}%s
*/

#include <CL/sycl.hpp>

#include <iostream>

using namespace cl::sycl;
using namespace cl::sycl::vendor::xilinx;

/// A small MathEngine program
template <typename Geography, typename ME_Array, int X, int Y>
struct tile
  : acap::me::tile<Geography, ME_Array, X, Y> {
  using t = acap::me::tile<Geography, ME_Array, X, Y>;
  using geo = Geography;

  void run(ME_Array &a) {
    std::cout << "Hello, I am the ME tile (" << X << ',' << Y
              << ") using " << sizeof(*this) << " bytes of memory "
              << std::endl;
    // Do not read at the start of the cascade
    if constexpr (!(t::x == geo::x_min && t::y == geo::y_min)) {
        auto cs_in = this->template get_cascade_stream_in<int>()
          .template get_access<access::mode::read,
                               access::target::blocking_pipe>();
       std::cout << "Reading " << cs_in.read() << std::endl;
    }
    // Do not write at the end of the cascade
    if constexpr (!(t::x == ((t::y & 1) ? geo::x_min : geo::x_max)
                    && t::y == geo::y_max)) {
        auto cs_out = t::template get_cascade_stream_out<int>()
          .template get_access<access::mode::write,
                               access::target::blocking_pipe>();
        cs_out << t::x*0x1000 + t::y;
    }
  }
};


int main() {
  std::cout << std::endl << "Instantiate big MathEngine:"
            << std::endl << std::endl;
  // acap::me::array<me_layout_1pe, tile> me;
  acap::me::array<acap::me::layout::full, tile> me;
  me.run();
}