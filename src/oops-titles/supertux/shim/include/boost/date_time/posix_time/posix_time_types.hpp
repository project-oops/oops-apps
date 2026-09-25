/*
 * `boost::posix_time::second_clock::local_time().date()` - today, for `GameConfig::is_christmas`.
 * `../gregorian/gregorian.hpp` has the whole account, including why "local" is UTC here.
 */
#ifndef STX_SHIM_BOOST_DATE_TIME_POSIX_TIME_TYPES_HPP
#define STX_SHIM_BOOST_DATE_TIME_POSIX_TIME_TYPES_HPP

#include <time.h>

#include <boost/date_time/gregorian/gregorian.hpp>

namespace boost {
namespace posix_time {

class ptime {
public:
  explicit ptime(const gregorian::date& d) : m_date(d) {}
  gregorian::date date() const { return m_date; }

private:
  gregorian::date m_date;
};

struct second_clock {
  static ptime local_time() {
    const time_t now = ::time(nullptr);
    struct tm t;
    if (!::gmtime_r(&now, &t)) return ptime(gregorian::date(1970, 1, 1));
    return ptime(gregorian::date(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday));
  }
  static ptime universal_time() { return local_time(); }
};

} // namespace posix_time
} // namespace boost

#endif
