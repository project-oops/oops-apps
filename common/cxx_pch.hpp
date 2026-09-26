/*
 * The standard headers nearly every C++ source in a ported title reaches, precompiled once per title
 * by `common/cxx.mk` and injected ahead of every source.
 *
 * Measured on Ship of Harkinian's `OTRGlobals.cpp`: 7,907 headers for one translation unit, 4,372 of
 * them libc++'s own and 2,126 the compiler's builtins - 82% of a parse that is 52 of the 63 seconds
 * that source takes, repeated across 536 sources.
 *
 * Only standard headers belong here. The value is in the ones everything includes, and a header whose
 * meaning depends on a macro a source sets just before including it would change meaning by arriving
 * first instead. A port's own headers, and a third-party header with configuration macros, are both
 * that shape; spdlog and ImGui are deliberately absent for it.
 *
 * A header that will not compile freestanding stops the precompile with its own error, which is the
 * right place to find out: the list below is what the titles here already include successfully.
 */
#ifndef OOPS_CXX_PCH_HPP
#define OOPS_CXX_PCH_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <optional>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#endif /* OOPS_CXX_PCH_HPP */
