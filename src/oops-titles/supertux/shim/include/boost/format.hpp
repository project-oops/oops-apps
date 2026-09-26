/*
 * `boost::format`, for the specifiers SuperTux's strings actually contain.
 *
 * Measured across `src/`: `%s` eleven times, `%d` twice, `%1%` once, and nothing else -
 * no width, no precision, no positional `%1$s`, and none in `data/locale/` either,
 * where a translator could have introduced one. So each argument is rendered the way
 * Boost renders it for those - through `operator<<` - and substituted in order; `%N%`
 * picks argument N; `%%` is a literal percent.
 *
 * **A specifier this does not understand is copied through, not guessed at.** A string
 * that shows `%5.2f` on screen is a visible, findable bug; one silently formatted wrong
 * is not. Boost throws on too few arguments; this leaves the specifier in the text for
 * the same reason.
 */
#ifndef STX_SHIM_BOOST_FORMAT_HPP
#define STX_SHIM_BOOST_FORMAT_HPP

#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace boost {

class format {
  public:
    explicit format(const std::string &fmt) : m_fmt(fmt) {}
    explicit format(const char *fmt) : m_fmt(fmt ? fmt : "") {}

    template <typename T> format &operator%(const T &value) {
        std::ostringstream out;
        out << value;
        m_args.push_back(out.str());
        return *this;
    }

    std::string str() const {
        std::string out;
        std::size_t next = 0;
        const std::size_t n = m_fmt.size();
        for (std::size_t i = 0; i < n; ++i) {
            const char c = m_fmt[i];
            if (c != '%' || i + 1 >= n) {
                out += c;
                continue;
            }
            const char d = m_fmt[i + 1];
            if (d == '%') {
                out += '%';
                ++i;
                continue;
            }
            if (d == 's' || d == 'd' || d == 'i' || d == 'u') {
                if (next < m_args.size()) {
                    out += m_args[next++];
                    ++i;
                    continue;
                }
                out += c;
                continue;
            }
            /* `%N%`: digits, then a closing percent. */
            std::size_t j = i + 1;
            std::size_t index = 0;
            while (j < n && m_fmt[j] >= '0' && m_fmt[j] <= '9') {
                index = index * 10 + static_cast<std::size_t>(m_fmt[j] - '0');
                ++j;
            }
            if (j > i + 1 && j < n && m_fmt[j] == '%' && index >= 1 &&
                index <= m_args.size()) {
                out += m_args[index - 1];
                i = j;
                continue;
            }
            out += c;
        }
        return out;
    }

  private:
    std::string m_fmt;
    std::vector<std::string> m_args;
};

inline std::string str(const format &f) {
    return f.str();
}

inline std::ostream &operator<<(std::ostream &os, const format &f) {
    return os << f.str();
}

} // namespace boost

#endif
