/*
 * `boost::algorithm::ends_with`, and its sibling for symmetry. SuperTux uses the first
 * twice, on file names. Case-sensitive, as Boost's is; the `iends_with` family is not
 * used.
 */
#ifndef STX_SHIM_BOOST_ALGORITHM_STRING_PREDICATE_HPP
#define STX_SHIM_BOOST_ALGORITHM_STRING_PREDICATE_HPP

#include <string>

namespace boost {
namespace algorithm {

inline bool ends_with(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline bool starts_with(const std::string &s, const std::string &prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

} // namespace algorithm

using algorithm::ends_with;
using algorithm::starts_with;

} // namespace boost

#endif
