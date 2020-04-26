/*
 * Implementation of support functions used by the main code.
 */
#include "support.h"

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
 * construct a local file name in /var/lock/localflock to use for the lock
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

