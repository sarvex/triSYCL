#ifndef TRISYCL_SYCL_VENDOR_XILINX_ACAP_AIE_TILE_BASE_HPP
#define TRISYCL_SYCL_VENDOR_XILINX_ACAP_AIE_TILE_BASE_HPP

/** \file

    The basic AI Engine homogeneous tile, with the common
    infrastructure to all the tiles (i.e. independent of x & y
    coordinates)

    Ronan dot Keryell at Xilinx dot com

    This file is distributed under the University of Illinois Open Source
    License. See LICENSE.TXT for details.
*/

#include <thread>

#include "axi_stream_switch.hpp"

namespace cl::sycl::vendor::xilinx::acap::aie {

/// \ingroup aie
/// @{

/** The AI Engine tile infrastructure common to all the tiles

    This allows some type erasure while accessing the common
    tile infrastructure.
*/
struct tile_base {
  /// The thread used to run this tile
  std::thread thread;

  // The AXI stream
  axi_stream_switch axi_ss;


  /** Get the input port

      \param[in] InputT is the data type to be used in the transfers

      \param[in] Target specifies if the connection is blocking or
      not.  It is blocking by default
  */
  template <typename T, access::target Target = access::target::blocking_pipe>
  auto in(int port) {
    return axi_ss.in[port].in<T, Target>();
  }


  /** Get the output port

      \param[in] InputT is the data type to be used in the transfers

      \param[in] Target specifies if the connection is blocking or
      not.  It is blocking by default
  */
  template <typename T, access::target Target = access::target::blocking_pipe>
  auto out(int port) {
    return axi_ss.out[port].out<T, Target>();
  }


  /** Provide a run member function that does nothing so it is
      possible to write a minimum AI Engine program that does nothing.

      Note that even if this function is not virtual, in the common
      case a programmer implements it to specify the program executed
      by a tile
  */
  void run() {
  }

};

/// @} End the aie Doxygen group

}

/*
    # Some Emacs stuff:
    ### Local Variables:
    ### ispell-local-dictionary: "american"
    ### eval: (flyspell-prog-mode)
    ### End:
*/

#endif // TRISYCL_SYCL_VENDOR_XILINX_ACAP_AIE_TILE_BASE_HPP