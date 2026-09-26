/*
 * `boost::filesystem`, for the operations SuperTux performs, over the POSIX shim.
 *
 * Users: `util/file_system.cpp` (`exists`, `is_directory`, `create_directory`,
 * `relative`, `remove`) and `supertux/main.cpp`, which canonicalises the data
 * directory and migrates a pre-0.4 config directory (`directory_iterator`, `rename`,
 * `remove_all`).
 *
 * This libc++ builds none of `<filesystem>`'s operations. `stat`, `mkdir`, `opendir`
 * and `getcwd` come from `common/posix`, `rename` and `remove` from oops-sdk's
 * `<stdio.h>`.
 *
 * Semantics are Boost's where SuperTux can observe them: `create_directory` returns
 * false for an existing directory, and failures Boost throws for throw
 * `filesystem_error`, which `main.cpp` catches by that name.
 */
#ifndef STX_SHIM_BOOST_FILESYSTEM_HPP
#define STX_SHIM_BOOST_FILESYSTEM_HPP

#include <cstdint>
#include <cstdio>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace boost {
namespace system {
/* `exists(path, ec)` takes one to mean "do not throw". Nothing reads it back. */
struct error_code {
    int value = 0;
    explicit operator bool() const { return value != 0; }
};
} // namespace system

namespace filesystem {

class filesystem_error : public std::runtime_error {
  public:
    explicit filesystem_error(const std::string &what) : std::runtime_error(what) {}
};

class path {
  public:
    path() = default;
    path(const char *s) : m_s(s ? s : "") {}
    path(const std::string &s) : m_s(s) {}

    const std::string &string() const { return m_s; }
    const char *c_str() const { return m_s.c_str(); }

    /* The last component, as Boost gives it: "/a/b" -> "b". */
    path filename() const {
        const std::string::size_type p = m_s.find_last_of('/');
        return p == std::string::npos ? path(m_s) : path(m_s.substr(p + 1));
    }

    path &operator/=(const path &rhs) {
        if (!m_s.empty() && m_s.back() != '/' && !rhs.m_s.empty() &&
            rhs.m_s.front() != '/')
            m_s += '/';
        m_s += rhs.m_s;
        return *this;
    }

    /* Boost uses this to pick the narrow/wide conversion for a platform's paths. They
     * are narrow UTF-8 here throughout, so there is nothing to choose. */
    static std::locale imbue(const std::locale &loc) { return loc; }

  private:
    std::string m_s;
};

inline path operator/(path lhs, const path &rhs) {
    return lhs /= rhs;
}
inline bool operator==(const path &a, const path &b) {
    return a.string() == b.string();
}
inline bool operator!=(const path &a, const path &b) {
    return !(a == b);
}

namespace detail {
inline std::vector<std::string> split(const std::string &s) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == '/') {
            if (!cur.empty())
                parts.push_back(cur);
            cur.clear();
        } else
            cur += c;
    }
    if (!cur.empty())
        parts.push_back(cur);
    return parts;
}

/* Absolute, with `.` and `..` resolved lexically. There are no symbolic links on the
 * console's sandboxed filesystem for a real `realpath` to follow. */
inline std::vector<std::string> normalise(const std::string &s) {
    std::string abs = s;
    if (abs.empty() || abs.front() != '/') {
        char buf[1024];
        const char *cwd = ::getcwd(buf, sizeof(buf));
        abs = std::string(cwd ? cwd : "/") + "/" + s;
    }
    std::vector<std::string> out;
    for (const std::string &p : split(abs)) {
        if (p == ".")
            continue;
        if (p == "..") {
            if (!out.empty())
                out.pop_back();
            continue;
        }
        out.push_back(p);
    }
    return out;
}

inline std::string join(const std::vector<std::string> &parts, std::size_t from = 0) {
    std::string s;
    for (std::size_t i = from; i < parts.size(); ++i) {
        if (!s.empty())
            s += '/';
        s += parts[i];
    }
    return s;
}
} // namespace detail

inline bool exists(const path &p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0;
}

inline bool exists(const path &p, system::error_code &ec) {
    ec.value = 0;
    return exists(p);
}

inline bool is_directory(const path &p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

inline bool create_directory(const path &p) {
    if (is_directory(p))
        return false;
    if (::mkdir(p.c_str(), 0755) != 0)
        throw filesystem_error("create_directory: " + p.string());
    return true;
}

inline bool remove(const path &p) {
    if (!exists(p))
        return false;
    return std::remove(p.c_str()) == 0;
}

inline void rename(const path &from, const path &to) {
    if (std::rename(from.c_str(), to.c_str()) != 0)
        throw filesystem_error("rename: " + from.string() + " -> " + to.string());
}

inline path canonical(const path &p) {
    if (!exists(p))
        throw filesystem_error("canonical: " + p.string() + " does not exist");
    return path("/" + detail::join(detail::normalise(p.string())));
}

/* `relative(p, base)`: the path from `base` to `p`, both made absolute first, as Boost
 * does. */
inline path relative(const path &p, const path &base) {
    const std::vector<std::string> a = detail::normalise(p.string());
    const std::vector<std::string> b = detail::normalise(base.string());
    std::size_t common = 0;
    while (common < a.size() && common < b.size() && a[common] == b[common])
        ++common;
    std::string out;
    for (std::size_t i = common; i < b.size(); ++i)
        out += out.empty() ? ".." : "/..";
    const std::string rest = detail::join(a, common);
    if (!rest.empty())
        out += out.empty() ? rest : "/" + rest;
    return path(out.empty() ? "." : out);
}

class directory_entry {
  public:
    directory_entry() = default;
    explicit directory_entry(const class path &p) : m_path(p) {}
    const class path &path() const { return m_path; }

  private:
    class path m_path;
};

/* An input iterator over one directory, skipping `.` and `..` as Boost does. The
 * entries are read up front, so the iterator is a plain value and nothing holds a
 * `DIR*` open. */
class directory_iterator {
  public:
    directory_iterator() = default;
    explicit directory_iterator(const path &dir) {
        DIR *d = ::opendir(dir.c_str());
        if (!d)
            throw filesystem_error("directory_iterator: " + dir.string());
        while (struct dirent *e = ::readdir(d)) {
            const std::string name = e->d_name;
            if (name == "." || name == "..")
                continue;
            m_entries.emplace_back(dir / name);
        }
        ::closedir(d);
        m_index = m_entries.empty() ? npos : 0;
    }

    const directory_entry &operator*() const { return m_entries[m_index]; }
    const directory_entry *operator->() const { return &m_entries[m_index]; }
    directory_iterator &operator++() {
        if (++m_index >= m_entries.size())
            m_index = npos;
        return *this;
    }
    bool operator==(const directory_iterator &o) const {
        return at_end() && o.at_end();
    }
    bool operator!=(const directory_iterator &o) const { return !(*this == o); }

  private:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    bool at_end() const { return m_index == npos; }

    std::vector<directory_entry> m_entries;
    std::size_t m_index = npos;
};

/* Depth first: a directory's contents, then the directory. POSIX's `remove` takes an
 * empty directory as well as a file; if the SDK's refuses one, this throws
 * `filesystem_error`, which is the failure `main.cpp`'s migration already catches and
 * logs. */
inline std::uintmax_t remove_all(const path &p) {
    std::uintmax_t n = 0;
    if (is_directory(p)) {
        for (directory_iterator it(p), end; it != end; ++it)
            n += remove_all(it->path());
    }
    if (exists(p)) {
        if (std::remove(p.c_str()) != 0)
            throw filesystem_error("remove_all: " + p.string());
        ++n;
    }
    return n;
}

} // namespace filesystem
} // namespace boost

#endif
