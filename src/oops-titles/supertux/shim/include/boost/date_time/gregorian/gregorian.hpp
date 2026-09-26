/*
 * `boost::gregorian::date`, for one comparison.
 *
 * `GameConfig::is_christmas` builds today's date and Saint Nicholas Day's - 6 December
 * of the same year - and answers `today >= saint_nicholas_day`, which puts Tux in a
 * Santa hat for the rest of December. That needs a date with a year, a month constant,
 * and an ordering; nothing else of Boost.Date_Time is reached.
 *
 * The date comes from oops-sdk's `gmtime_r`, which is UTC because the console hands a
 * payload no timezone - see `<time.h>`. A player an ocean from Greenwich gets the hat a
 * few hours early or late, on one day a year.
 */
#ifndef STX_SHIM_BOOST_DATE_TIME_GREGORIAN_HPP
#define STX_SHIM_BOOST_DATE_TIME_GREGORIAN_HPP

namespace boost {
namespace gregorian {

enum months_of_year { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

class date {
  public:
    date(int year, int month, int day) : m_year(year), m_month(month), m_day(day) {}

    int year() const { return m_year; }
    int month() const { return m_month; }
    int day() const { return m_day; }

    bool operator<(const date &o) const { return key() < o.key(); }
    bool operator>=(const date &o) const { return !(*this < o); }
    bool operator==(const date &o) const { return key() == o.key(); }

  private:
    long key() const { return (static_cast<long>(m_year) * 16 + m_month) * 32 + m_day; }

    int m_year;
    int m_month;
    int m_day;
};

} // namespace gregorian
} // namespace boost

#endif
