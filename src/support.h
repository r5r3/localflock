/*
 * Declaration of support functions used by the main code.
 */
#ifndef LOCALFLOCK_SUPPORT_H
#define LOCALFLOCK_SUPPORT_H

// standard functions
#include <string>
#include <cstdlib>
#include <filesystem>
using namespace std;

// SPDlog Header-Only Logger
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/fmt/fmt.h"
extern shared_ptr<spdlog::logger> logger;

// calculation of hashes for filenames used to obscure filenames
#include <gcrypt.h>
extern gcry_md_hd_t hash_context;

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
int open_and_set_perm(string &path);

// struct for settings
struct settings_t {
    // the directory to be used for lock files. Default: /var/lock/localflock.
    string LOCKDIR;
    // whether or not to show debug messages on stderr. Default: false.
    bool DEBUG;
    // whether or not to obscure files names. Default: false.
    bool SHOW_NAMES;
    // name for the protocol file. Default: $LOCKDIR/protocl, not changeable
    string PROTOCOL_FILE;
};
extern settings_t settings;

#endif //LOCALFLOCK_SUPPORT_H
