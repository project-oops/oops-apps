/*
 * tinygettext's export header, which its CMake generates with `GENERATE_EXPORT_HEADER`
 * to mark the library's API for a Windows DLL. Linked statically into one payload,
 * there is nothing to export or import, so the macros are empty - what CMake itself
 * generates for a static build.
 */
#ifndef TINYGETTEXT_EXPORT_H
#define TINYGETTEXT_EXPORT_H

#define TINYGETTEXT_API
#define TINYGETTEXT_NO_EXPORT
#define TINYGETTEXT_DEPRECATED
#define TINYGETTEXT_DEPRECATED_EXPORT
#define TINYGETTEXT_DEPRECATED_NO_EXPORT

#endif
