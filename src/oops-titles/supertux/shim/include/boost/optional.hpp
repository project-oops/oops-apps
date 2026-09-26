/*
 * `boost::optional`, as `std::optional` with the four spellings SuperTux uses that the
 * standard one does not have.
 *
 * Those are `get()`, `get_value_or`, `get_ptr()` and assignment from the in-place
 * factory `reader_mapping.cpp` uses. Deriving from `std::optional` keeps every other
 * operation the standard's.
 *
 * `../../../docs/PORTING.md`, The stack, explains why Boost is shimmed rather than
 * pinned.
 */
#ifndef STX_SHIM_BOOST_OPTIONAL_HPP
#define STX_SHIM_BOOST_OPTIONAL_HPP

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/utility/typed_in_place_factory.hpp>

/* `BOOST_FALLTHROUGH` is Boost's `[[fallthrough]]`, from `<boost/config.hpp>`. SuperTux
 * files reach it only through `util/reader_mapping.hpp`'s `<boost/optional.hpp>`. */
#ifndef BOOST_FALLTHROUGH
#define BOOST_FALLTHROUGH [[fallthrough]]
#endif

namespace boost {

using none_t = std::nullopt_t;
inline constexpr std::nullopt_t none = std::nullopt;

template <typename T> class optional;

namespace detail {
template <typename U> struct is_in_place_factory : std::false_type {};
template <typename T, typename... A>
struct is_in_place_factory<typed_in_place_factory<T, A...>> : std::true_type {};

template <typename U> struct is_boost_optional : std::false_type {};
template <typename T> struct is_boost_optional<optional<T>> : std::true_type {};
} // namespace detail

template <typename T> class optional : public std::optional<T> {
  public:
    using std::optional<T>::optional;

    optional() = default;
    optional(const optional &) = default;
    optional(optional &&) = default;
    optional &operator=(const optional &) = default;
    optional &operator=(optional &&) = default;

    /* Assignment is declared here rather than inherited: with the inherited converting
     * constructors, the base's `operator=(U&&)` and the defaulted copy would make
     * `opt = true` ambiguous. */
    optional &operator=(std::nullopt_t) noexcept {
        this->reset();
        return *this;
    }

    template <typename U = T,
              typename = std::enable_if_t<
                  !detail::is_boost_optional<std::decay_t<U>>::value &&
                  !detail::is_in_place_factory<std::decay_t<U>>::value &&
                  !std::is_same<std::decay_t<U>, std::nullopt_t>::value &&
                  std::is_constructible<T, U &&>::value>>
    optional &operator=(U &&value) {
        std::optional<T>::operator=(std::forward<U>(value));
        return *this;
    }

    /* `boost::optional<T> x = std::optional<T>{...}` and the reverse, so the two mix
     * freely. */
    optional(const std::optional<T> &o) : std::optional<T>(o) {}
    optional(std::optional<T> &&o) : std::optional<T>(std::move(o)) {}

    /* Constructs in place: `ReaderMapping` holds references and cannot be assigned, so
     * the factory carries its constructor arguments instead. */
    template <typename... A>
    optional &operator=(const typed_in_place_factory<T, A...> &f) {
        std::apply([this](const A &...a) { this->emplace(a...); }, f.args);
        return *this;
    }

    T &get() { return **this; }
    const T &get() const { return **this; }

    T *get_ptr() { return this->has_value() ? &**this : nullptr; }
    const T *get_ptr() const { return this->has_value() ? &**this : nullptr; }

    template <typename U> T get_value_or(U &&fallback) const {
        return this->value_or(std::forward<U>(fallback));
    }

    bool is_initialized() const { return this->has_value(); }
};

} // namespace boost

#endif
