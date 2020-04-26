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
using namespace std;

// this includes the dlsym to load the original function
#include <dlfcn.h>

// class for lock information and other support functions
#include "lock_info.h"
#include "support.h"
shared_ptr<spdlog::logger> logger;
original_flock_type originalFlock;
original_fcntl_type originalFcntl;
original_close_type originalClose;

// a dictionary to keep track of locks
map<int, shared_ptr<LockInfo>> lock_map;

/*
 * Cleanup function called when the program loading this library is closed. This is unfortunately not
 * always ensured.
 */
void __attribute__ ((destructor)) cleanup() {
    logger->info("cleanup");
    logger->info("cleaning up {} locks...", lock_map.size());

    // loop over all remaining locks and remove them
    for (const auto& one_lock: lock_map) {
        logger->info("removing lock for fd={0}, path={1}", one_lock.first, one_lock.second->orignal_path);
        one_lock.second->cleanup();
    }
}

/*
 * initialize internal structures and pointers to original functions
 */
void __attribute__ ((constructor)) init() {
    // get pointers to the original system functions.
    originalFlock = (original_flock_type)dlsym(RTLD_NEXT, "flock");
    originalFcntl = (original_fcntl_type)dlsym(RTLD_NEXT, "fcntl");
    originalClose = (original_close_type)dlsym(RTLD_NEXT, "close");

    // register a logger
    logger = spdlog::stderr_logger_st("localflock");

    // create the local folder for locks.
    if (!filesystem::is_directory("/var/lock/localflock")) {
        filesystem::create_directory("/var/lock/localflock");
        filesystem::permissions("/var/lock/localflock", filesystem::perms::all);
    }
}

/*
 * Replacement for the flock function.
 */
extern "C" int flock(int fd, int operation) {
    logger->info("flock({0}, {1})", fd, operation);

    // if this file is not already in our lock_map, create
    if (lock_map.count(fd) == 0) {
        // store information about this files. We also create the local lock file here
        lock_map[fd] = shared_ptr<LockInfo>(new LockInfo(fd));
        logger->info("number of locks: {}", lock_map.size());
        logger->info("\n{}", lock_map[fd]->str());
    }

    // perform the operation on the local file
    logger->info("calling flock for {}", lock_map[fd]->local_fd);
    return originalFlock(lock_map[fd]->local_fd, operation);
}


/*
 * Replacement for the fcntl function. This function is used for different things. We decide based
 * on the second argument whether to call the original fcntl on the original or the lock file.
 */
extern "C" int fcntl(int fd, int operation, ...) {
    logger->info("fcntl({0}, {1}, ...)", fd, operation);
    // we have to deal with a variable list of arguments. The third argument is optional.
    // and can have different types.
    struct flock* arg_flock;
    struct f_owner_ex* arg_f_owner_ex;
    int arg_int;
    uint64_t* arg_uint64_t;
    int result = -1;
    va_list vl;
    va_start(vl, operation);
    switch (operation) {
        // cases with integer argument
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETFL:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ:
        case F_ADD_SEALS:
            logger->info("    -> forwarding integer arg for original file");
            arg_int = va_arg(vl, int);
            result = originalFcntl(fd, operation, arg_int);
            break;
        // cases with uint64_t pointer
        case F_GET_RW_HINT:
        case F_SET_RW_HINT:
        case F_GET_FILE_RW_HINT:
        case F_SET_FILE_RW_HINT:
            logger->info("    -> forwarding uint64_t* arg for original file");
            arg_uint64_t = va_arg(vl, uint64_t*);
            result = originalFcntl(fd, operation, arg_uint64_t);
            break;
        // cases with void argument
        case F_GETFD:
        case F_GETFL:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ:
        case F_GET_SEALS:
            logger->info("    -> forwarding no arg for original file");
            result = originalFcntl(fd, operation);
            break;
        // cases with f_owner_ex
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            logger->info("    -> forwarding f_owner_ex* arg for original file");
            arg_f_owner_ex = va_arg(vl, f_owner_ex*);
            result = originalFcntl(fd, operation, arg_f_owner_ex);
            break;
        // cases with flock we are interested in
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
        case F_OFD_SETLK:
        case F_OFD_SETLKW:
        case F_OFD_GETLK:
            arg_flock = va_arg(vl, struct flock*);

            // if this file is not already in our lock_map, create
            if (lock_map.count(fd) == 0) {
                // store information about this files. We also create the local lock file here
                lock_map[fd] = shared_ptr<LockInfo>(new LockInfo(fd));
                logger->info("number of locks: {}", lock_map.size());
                logger->info("\n{}", lock_map[fd]->str());
            }

            // perform the operation on the local file
            logger->info("    -> forwarding flock* arg for local file {}", lock_map[fd]->local_path);
            result = originalFcntl(lock_map[fd]->local_fd, operation, arg_flock);
            break;
        // for the default case, we assume no argument
        default:
            logger->warn("    -> unknown command called on original file");
            result = originalFcntl(fd, operation);
            break;
    }
    va_end(vl);
    logger->info("    -> result: {}", result);
    return result;
}

/*
 * replacement for the close function.
 */
extern "C" int close(int fd) {
    logger->info("close({0})", fd);

    // close the file and free the lock information if this is a known locked file.
    if (lock_map.count(fd) > 0) {
        logger->info("    -> {} is locked!", lock_map[fd]->orignal_path);
        lock_map[fd]->cleanup();
        lock_map.erase(fd);
    }

    // perform the actual close operation on the original file
    return originalClose(fd);
}
