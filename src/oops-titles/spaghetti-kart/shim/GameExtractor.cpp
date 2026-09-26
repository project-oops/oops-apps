/*
 * `GameExtractor`, in place of `upstream/src/port/GameExtractor.cpp`.
 *
 * Upstream's converts a Mario Kart 64 ROM into `mk64.o2r` through torch, and torch is a host tool:
 * yaml-cpp, GSL, its own N64 graphics and binary readers, none of which run in a frame. This build
 * ships no ROM and does no conversion, so the extractor's job here is to say what is missing and
 * where it goes, in the log and in a dialog the player can read on screen.
 *
 * It still finds ROMs on disk, because that is the one part worth keeping honest: the message names
 * a `.z64` it can see, or says none is there, rather than guessing.
 *
 * `GenerateOTR` returning false is the failure upstream already handles - `GenerateAssetsMods()`
 * reports it and exits - so nothing here pretends to have produced an archive.
 */
#include "GameExtractor.h"

#include "Engine.h"

#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "oops/system.h"

#ifndef OOPS_POSIX_HOME
#define OOPS_POSIX_HOME "/data/spaghetti-kart"
#endif

/* The archive the game asks for, from `Engine.h`, named here so the message can quote it. */
static const char *const kGameArchive = "mk64.o2r";

void GameExtractor::GetRoms(std::vector<std::string>& roms) {
    DIR *d = opendir(OOPS_POSIX_HOME);
    if (d == nullptr) {
        return;
    }
    for (struct dirent *entry = readdir(d); entry != nullptr; entry = readdir(d)) {
        const std::string name(entry->d_name);
        if (name.size() < 5 || name.compare(name.size() - 4, 4, ".z64") != 0) {
            continue;
        }
        const std::string full = std::string(OOPS_POSIX_HOME) + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            roms.push_back(full);
        }
    }
    closedir(d);
}

bool GameExtractor::SelectGameFromUI() {
    std::vector<std::string> roms;
    GetRoms(roms);

    std::string message = std::string(kGameArchive) + " is missing.\n\n";
    if (roms.empty()) {
        message += "No .z64 ROM was found in " OOPS_POSIX_HOME " either.\n\n"
                   "Please provide your own Mario Kart 64 ROM: convert it to " +
                   std::string(kGameArchive) + " on a desktop and copy that file to " +
                   OOPS_POSIX_HOME ".";
        oops_log_error("SPGK", "%s missing, and no .z64 in %s", kGameArchive, OOPS_POSIX_HOME);
    } else {
        message += "A ROM is present (" + roms.front() +
                   ") but this build does not convert one.\n\n"
                   "Convert it to " + std::string(kGameArchive) + " on a desktop and copy that "
                   "file to " OOPS_POSIX_HOME ".";
        oops_log_error("SPGK", "%s missing; %s is present but conversion is a desktop step",
                       kGameArchive, roms.front().c_str());
    }

    GameEngine::ShowMessage("Missing game data", message.c_str(), SDL_MESSAGEBOX_ERROR);
    return false;
}

/*
 * Never reached: `GameEngine::GenAssetFile()` exits on `SelectGameFromUI` returning false. Defined
 * because the header declares it and a link that lost it would only say so at run time.
 */
std::optional<std::string> GameExtractor::ValidateChecksum() const {
    return std::nullopt;
}

bool GameExtractor::GenerateOTR() const {
    oops_log_error("SPGK", "no archive was generated: conversion is a desktop step");
    return false;
}
