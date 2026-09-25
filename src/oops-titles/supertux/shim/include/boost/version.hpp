/*
 * `BOOST_VERSION`, which `util/file_system.cpp` tests once: at 1.60 or later it calls
 * `fs::relative`, below that it walks two paths with iterators. The shim's `filesystem.hpp`
 * provides `relative`, so this claims a version past that line.
 *
 * The number is a statement about the API this shim offers, not about any Boost release.
 */
#ifndef STX_SHIM_BOOST_VERSION_HPP
#define STX_SHIM_BOOST_VERSION_HPP

#define BOOST_VERSION 106000

#endif
