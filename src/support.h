/*
 * Declaration of support functions used by the main code.
 */
#ifndef LOCALFLOCK_SUPPORT_H
#define LOCALFLOCK_SUPPORT_H

// standard functions
#include <string>
#include <filesystem>
using namespace std;

// SPDlog Header-Only Logger
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/fmt/fmt.h"
extern shared_ptr<spdlog::logger> logger;

// Type definition for overwritten function
#include <fcntl.h>
#include <cstdarg>
typedef int (*original_flock_type)(int, int);
typedef int (*original_fcntl_type)(int, int, ...);
typedef int (*original_close_type)(int);
extern original_flock_type originalFlock;
extern original_fcntl_type originalFcntl;
extern original_close_type originalClose;

// own functions
string get_path_for_fd(int fd);
string get_local_lock_path(string &path);

#endif //LOCALFLOCK_SUPPORT_H
