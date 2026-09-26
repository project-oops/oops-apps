/*
 * `boost::locale::generator().generate("")`, SuperTux's one use of Boost.Locale.
 *
 * `main.cpp` makes the environment's locale global at start-up so that Boost.Filesystem
 * converts paths through it. The environment has no locale here - no `LANG`, no locale
 * database - and paths are narrow UTF-8 throughout, so the answer Boost would reach is
 * the classic locale. SuperTux's *translations* do not come from this: they are
 * tinygettext's, chosen by the config's `locale` setting and `findlocale`.
 */
#ifndef STX_SHIM_BOOST_LOCALE_HPP
#define STX_SHIM_BOOST_LOCALE_HPP

#include <locale>
#include <string>

namespace boost {
namespace locale {

class generator {
  public:
    std::locale generate(const std::string &) const { return std::locale::classic(); }
};

} // namespace locale
} // namespace boost

#endif
