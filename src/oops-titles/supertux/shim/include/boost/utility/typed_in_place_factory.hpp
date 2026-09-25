/*
 * `boost::in_place<T>(args...)` - a bundle of constructor arguments for `boost::optional<T>` to
 * build a `T` from where it lives. SuperTux's one use is `reader_mapping.cpp`, constructing a
 * `ReaderMapping`, which holds references and so cannot be assigned. See `../optional.hpp`.
 */
#ifndef STX_SHIM_BOOST_TYPED_IN_PLACE_FACTORY_HPP
#define STX_SHIM_BOOST_TYPED_IN_PLACE_FACTORY_HPP

#include <tuple>

namespace boost {

template <typename T, typename... A>
struct typed_in_place_factory {
  std::tuple<A...> args;
};

template <typename T, typename... A>
typed_in_place_factory<T, A...> in_place(const A&... a) {
  return typed_in_place_factory<T, A...>{std::tuple<A...>(a...)};
}

} // namespace boost

#endif
