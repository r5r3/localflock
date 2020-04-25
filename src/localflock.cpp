/*
 * Create all flocks locally.
 *
 * Author: Robert Redl
 *
 * Build Dependencies on Ubuntu: libspdlog-dev
 */

// We want to find the original implementation of a function, which is not our implementation.
// To do that, we need RTLD_NEXT.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// headers with used standard functions
#include <iostream>
#include <cstdlib>
#include <filesystem>
using namespace std;

// this includes the dlsym to load the original function
#include <dlfcn.h>

// SPDlog Header-Only Logger
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/fmt/fmt.h"
std::shared_ptr<spdlog::logger> logger;

// Type definition for overwritten function
typedef int (*original_flock_type)(int, int);
typedef int (*original_close_type)(int);
original_flock_type originalFlock;
original_close_type originalClose;

// a dictionary to keep track of locks
map<int, string> lock_map;

/*
 * Cleanup function called when the program loading this library is closed. This is unfortunately not
 * always ensured.
 */
void __attribute__ ((destructor)) cleanup() {
    logger->info("cleanup");
    logger->info("cleaning up {} locks...", lock_map.size());

    // loop over all remaining locks and remove them
    for (const auto& one_lock: lock_map) {
        logger->info("removing lock for fd={0}, path={1}", one_lock.first, one_lock.second);
    }
}

/*
 * check /proc/self/fd/ to get the absolute path of the file to lock
 */
string get_path_for_fd(int fd) {
    string result;
    try {
        result = filesystem::read_symlink(fmt::format("/proc/self/fd/{}", fd));
    } catch(exception& e){
        logger->warn("get_path_for_fd: unable to get path for {}, no lock will be created!", fd);
        logger->warn(e.what());
        result = "";
    }
    return result;
}

/*
 * contruct a local file name in /var/lock/localflock to use for the lock
 */
string get_local_lock_path(string &path) {
    string suffix;
    suffix.assign(path, 1, string::npos);
    for (int i=0; i < suffix.size(); i++) {
        if (suffix[i] == '/') suffix[i] = '-';
    }
    string result = fmt::format("/var/lock/localflock/{}", suffix);
    return result;
}

/*
 * initialize internal structures and pointers to original functions
 */
void __attribute__ ((constructor)) init() {
    // get pointers to the original system functions.
    originalFlock = (original_flock_type)dlsym(RTLD_NEXT, "flock");
    originalClose = (original_close_type)dlsym(RTLD_NEXT, "close");

    // register a logger
    logger = spdlog::stderr_logger_st("localflock");
}

/*
 * Replacement for the flock function.
 */
extern "C" int flock(int fd, int operation) {
    logger->info("flock({0}, {1})", fd, operation);

    // the get absolute path to the file, do nothing if this fails
    string abs_path = get_path_for_fd(fd);
    if (abs_path == "") return 1;
    logger->info("    -> {}", abs_path);

    // store the path for later lock removal
    lock_map[fd] = abs_path;
    logger->info("number of locks: {}", lock_map.size());

    // create the temporal lock file
    string tmp_name = get_local_lock_path(abs_path);
    logger->info("    -> {}", tmp_name);
    return originalFlock(fd, operation);
}

/*
 * replacement for the close function.
 */
extern "C" int close(int fd) {
    logger->info("close({0})", fd);
    string abs_path = get_path_for_fd(fd);
    logger->info("    -> {}", abs_path);
    return originalClose(fd);
}
