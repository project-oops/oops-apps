/*
 * `Extractor`, in place of `upstream/soh/soh/Extractor/Extract.cpp`.
 *
 * Upstream converts an Ocarina of Time ROM into `oot.o2r` on the machine that runs the game, and it
 * does so by calling `zapd_report` - ZAPD linked in as a library, 93 sources, with OTRExporter's 26
 * behind it, a temporary directory, a symbolic link to the asset tree and a working-directory
 * change. None of that is in this payload, so the seven methods the game calls are here instead, and
 * each says what it did rather than reporting a success it did not have.
 *
 * What the player sees: the archive that is missing, and where to put one. The ROM search is real -
 * the message names a `.z64` it can see, or says none is there - because a message that guesses is
 * worse than none.
 *
 * `../ISSUES.md` carries the on-device conversion as open work. `OTRGlobals.cpp:645` is the branch
 * this lands in: `ManuallySearchForRomMatchingType` returning false moves it to its file check and
 * then to the "Main OTR file not found" path libultraship already has.
 */
/* `soh/` first: upstream's own sources reach this header from the directory it sits in, and this
 * file is not in that directory. `upstream/soh` is on the include path. */
#include "soh/Extractor/Extract.h"

#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <SDL2/SDL.h>

#include "oops/system.h"

#ifndef OOPS_POSIX_HOME
#define OOPS_POSIX_HOME "/data/soh"
#endif

/* The two archives the game asks for, from `OTRGlobals.cpp`, named so the message can quote them. */
static const char* const kVanillaArchive = "oot.o2r";
static const char* const kMasterQuestArchive = "oot-mq.o2r";

void Extractor::SetSearchPath(const std::string& path) {
    mSearchPath = path;
}

void Extractor::GetRoms(std::vector<std::string>& roms) {
    const std::string dir = mSearchPath.empty() ? std::string(OOPS_POSIX_HOME) : mSearchPath;
    DIR* d = opendir(dir.c_str());
    if (d == nullptr) {
        return;
    }
    for (struct dirent* entry = readdir(d); entry != nullptr; entry = readdir(d)) {
        const std::string name(entry->d_name);
        if (name.size() < 5 || name.compare(name.size() - 4, 4, ".z64") != 0) {
            continue;
        }
        const std::string full = dir + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            roms.push_back(full);
        }
    }
    closedir(d);
}

bool Extractor::IsMasterQuest() const {
    /* Nothing was converted, so neither archive was produced, and the vanilla answer is the one
     * that sends the caller to the file it looks for first. */
    return false;
}

bool Extractor::ManuallySearchForRomMatchingType(RomSearchMode searchMode) {
    (void)searchMode;

    std::vector<std::string> roms;
    GetRoms(roms);

    std::string message = std::string(kVanillaArchive) + " is missing.\n\n";
    if (roms.empty()) {
        message += "No .z64 ROM was found in " OOPS_POSIX_HOME " either.\n\n"
                   "Please provide your own Ocarina of Time ROM: convert it to " +
                   std::string(kVanillaArchive) + " on a desktop and copy that file to " +
                   OOPS_POSIX_HOME ". " + kMasterQuestArchive + " is the Master Quest equivalent.";
        oops_log_error("SOH", "%s missing, and no .z64 in %s", kVanillaArchive, OOPS_POSIX_HOME);
    } else {
        message += "A ROM is present (" + roms.front() +
                   ") but this build does not convert one.\n\n"
                   "Convert it to " + std::string(kVanillaArchive) +
                   " on a desktop and copy that file to " OOPS_POSIX_HOME ".";
        oops_log_error("SOH", "%s missing; %s is present but conversion is a desktop step",
                       kVanillaArchive, roms.front().c_str());
    }

    ShowErrorBox("Missing game data", message.c_str());
    return false;
}

bool Extractor::RunFileStandalone(std::string file) {
    oops_log_error("SOH", "cannot convert %s: the converter is not in this payload", file.c_str());
    return false;
}

bool Extractor::CallZapd(std::string installPath, std::string exportdir,
                         std::atomic<size_t>* extractCount, std::atomic<size_t>* totalExtract) {
    (void)installPath;
    (void)exportdir;
    /* Left at zero rather than set to anything: the progress window reads these, and a count that
     * moved would show extraction happening. */
    (void)extractCount;
    (void)totalExtract;
    oops_log_error("SOH", "no archive was generated: conversion is a desktop step");
    return false;
}

void Extractor::ShowErrorBox(const char* title, const char* text) {
    oops_log_error("SOH", "%s: %s", title ? title : "(no title)", text ? text : "");
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, nullptr);
}
