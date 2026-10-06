/*
 * `Companion.h`, as much of it as this build needs.
 *
 * Upstream's is torch's: the asset converter that reads a Mario Kart 64 ROM and writes `mk64.o2r`.
 * torch is a host tool with its own dependency set - yaml-cpp, GSL, its own N64 graphics and binary
 * readers - and it runs once, on a desktop, to produce a file. Nothing in a frame reaches it.
 *
 * The one source that includes it, `src/port/GameExtractor.cpp`, is replaced by
 * `shim/GameExtractor.cpp`, which says where the archive should be instead of building one. This
 * header exists because `src/port/GameExtractor.h` is still the declaration both sides compile
 * against, and it opens with `#include "Companion.h"` for the `fs` alias in its member list.
 *
 * It shadows torch's copy, so `upstream/torch/src` is off the include path: a source that needs
 * more of torch than the alias fails to compile here rather than quietly pulling the tool in.
 */
#ifndef OOPS_SPGK_COMPANION_H
#define OOPS_SPGK_COMPANION_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#endif /* OOPS_SPGK_COMPANION_H */
