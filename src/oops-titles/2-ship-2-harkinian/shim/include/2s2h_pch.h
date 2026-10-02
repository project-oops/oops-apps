#pragma once

#ifdef __cplusplus
extern "C" long lroundf(float x);
#else
long lroundf(float x);
#endif

#ifdef __cplusplus
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/window/Window.h>
#include <ship/window/gui/ConsoleWindow.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <functional>
#include <variant>
#include <nlohmann/json.hpp>

#ifndef ZAPD_BUILD
#include "2s2h/GameInteractor/GameInteractor.h"
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "z64.h"
#endif
#endif

