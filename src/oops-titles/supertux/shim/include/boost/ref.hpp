/*
 * `boost::ref`, which C++11 adopted as `std::ref`. SuperTux passes two through
 * `boost::in_place`, and a `std::reference_wrapper` converts to the `const T&` the constructor
 * takes exactly as Boost's does.
 */
#ifndef STX_SHIM_BOOST_REF_HPP
#define STX_SHIM_BOOST_REF_HPP

#include <functional>

namespace boost {
using std::cref;
using std::ref;
using std::reference_wrapper;
} // namespace boost

#endif
