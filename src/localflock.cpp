/*
 * Create all flocks locally.
 *
 * Author: Robert Redl
 *
 * Build Dependencies on Ubuntu: libspdlog-dev, libgcrypt20-dev
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
#include <memory>

// some functionality of fnctl depends on the linux kernel version
#include <linux/version.h>

// class for lock information and other support functions
#include "lock_info.h"
#include "support.h"
#include "protocol.h"
shared_ptr<spdlog::logger> logger;
original_flock_type originalFlock;
original_fcntl_type originalFcntl;
original_close_type originalClose;
bool init_called = false;
mutex mtx;

// a dictionary to keep track of locks and one for settings. The init priority makes sure that they are
// initialized before calling the init method.
map<int, shared_ptr<LockInfo>> lock_map __attribute__ ((init_priority (101)));
shared_ptr<settings_t> settings __attribute__ ((init_priority (101)));

/*
 * Cleanup function called when the program loading this library is closed. This is unfortunately not
 * always ensured.
 */
void __attribute__ ((destructor)) localflock_cleanup() {
    logger->debug("cleanup");

    // loop over all remaining locks and remove them
    if (lock_map.size() > 0) logger->debug("cleaning up {} locks...", lock_map.size());
    for (const auto& one_lock: lock_map) {
        logger->debug("removing lock for fd={0}, path={1}", one_lock.first, one_lock.second->orignal_path);
        one_lock.second->cleanup();
    }
}

/*
 * initialize internal structures and pointers to original functions
 */
void __attribute__ ((constructor (102))) localflock_init() {
    if (init_called) return;
    mtx.lock();

    // get pointers to the original system functions.
    originalFlock = (original_flock_type)dlsym(RTLD_NEXT, "flock");
    originalFcntl = (original_fcntl_type)dlsym(RTLD_NEXT, "fcntl");
    originalClose = (original_close_type)dlsym(RTLD_NEXT, "close");

    // register a logger
    logger = spdlog::stderr_logger_st("localflock");

    // read settings from environment variables
    settings = make_shared<settings_t>();
    const char* value = getenv("LOCALFLOCK_DEBUG");
    if (value == nullptr) {
        settings->DEBUG = false;
    } else {
        settings->DEBUG = true;
        logger->set_level(spdlog::level::debug);
    }
    logger->debug("LOCALFLOCK_DEBUG={}", settings->DEBUG);
    value = getenv("LOCALFLOCK_LOCKDIR");
    if (value == nullptr) {
        settings->LOCKDIR = "/var/lock/localflock";
    } else {
        settings->LOCKDIR = string(value);
    }
    settings->PROTOCOL_FILE = fmt::format("{}/protocol", settings->LOCKDIR);
    logger->debug("LOCALFLOCK_LOCKDIR={}", settings->LOCKDIR);
    value = getenv("LOCALFLOCK_SHOW_NAMES");
    if (value == nullptr) {
        settings->SHOW_NAMES = false;
        // init hashing library here
        gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
        gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    } else {
        settings->SHOW_NAMES = true;
    }
    logger->debug("LOCALFLOCK_SHOW_NAMES={}", settings->SHOW_NAMES);

    // create the local folder for locks.
    if (!filesystem::is_directory(settings->LOCKDIR)) {
        filesystem::create_directory(settings->LOCKDIR);
        filesystem::permissions(settings->LOCKDIR, filesystem::perms::all);
    }

    // cleanup old files from the lock directory.
    Protocol* proto = new Protocol();
    proto->cleanup();
    proto->close();
    delete proto;

    // now we are done with initialisation
    init_called = true;
    mtx.unlock();
}

/*
 * make sure that the init function has already been called.
 */
void wait_for_init() {
    int waited = 0;
    while (!init_called && waited < 1000) {
        this_thread::sleep_for(chrono::milliseconds(1));
        waited++;
    }
    if (!init_called) localflock_init();
}

/*
 * Replacement for the flock function.
 */
extern "C" int flock(int fd, int operation) {
    wait_for_init();
    logger->debug("flock({0}, {1})", fd, operation);

    // if this file is not already in our lock_map, create
    if (lock_map.count(fd) == 0) {
        // store information about this files. We also create the local lock file here
        lock_map[fd] = std::make_shared<LockInfo>(fd);
        logger->debug(lock_map[fd]->str());
    }

    // perform the operation on the local file
    logger->debug("    -> calling flock for local file {}", lock_map[fd]->local_path);
    return originalFlock(lock_map[fd]->local_fd, operation);
}


/*
 * Replacement for the fcntl function. This function is used for different things. We decide based
 * on the second argument whether to call the original fcntl on the original or the lock file.
 */
extern "C" int fcntl(int fd, int operation, ...) {
    wait_for_init();
    logger->debug("fcntl({0}, {1}, ...)", fd, operation);
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
        case F_ADD_SEALS:
#endif
            logger->debug("    -> forwarding integer arg for original file");
            arg_int = va_arg(vl, int);
            result = originalFcntl(fd, operation, arg_int);
            break;
        // cases with uint64_t pointer
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
        case F_GET_RW_HINT:
        case F_SET_RW_HINT:
        case F_GET_FILE_RW_HINT:
        case F_SET_FILE_RW_HINT:
            logger->debug("    -> forwarding uint64_t* arg for original file");
            arg_uint64_t = va_arg(vl, uint64_t*);
            result = originalFcntl(fd, operation, arg_uint64_t);
            break;
#endif
        // cases with void argument
        case F_GETFD:
        case F_GETFL:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
        case F_GET_SEALS:
#endif
            logger->debug("    -> forwarding no arg for original file");
            result = originalFcntl(fd, operation, NULL);
            break;
        // cases with f_owner_ex
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            logger->debug("    -> forwarding f_owner_ex* arg for original file");
            arg_f_owner_ex = va_arg(vl, f_owner_ex*);
            result = originalFcntl(fd, operation, arg_f_owner_ex);
            break;
        // cases with flock we are interested in
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
        case F_OFD_SETLK:
        case F_OFD_SETLKW:
        case F_OFD_GETLK:
#endif
            arg_flock = va_arg(vl, struct flock*);

            // if this file is not already in our lock_map, create
            if (lock_map.count(fd) == 0) {
                // store information about this files. We also create the local lock file here
                lock_map[fd] = std::make_shared<LockInfo>(fd);
                logger->debug(lock_map[fd]->str());
            }

            // perform the operation on the local file
            logger->debug("    -> forwarding flock* arg for local file {}", lock_map[fd]->local_path);
            result = originalFcntl(lock_map[fd]->local_fd, operation, arg_flock);
            break;
        // for the default case, we assume no argument
        default:
            logger->warn("    -> unknown command called on original file");
            result = originalFcntl(fd, operation);
            break;
    }
    va_end(vl);
    logger->debug("    -> result: {}", result);
    return result;
}

/*
 * replacement for the close function.
 */
extern "C" int close(int fd) {
    wait_for_init();
    // close the file and free the lock information if this is a known locked file.
    if (lock_map.count(fd) > 0) {
        logger->debug("close({0})", fd);
        logger->debug("    -> {} is known!", lock_map[fd]->orignal_path);
        logger->debug("    -> {} is also closed!", lock_map[fd]->local_path);
        lock_map[fd]->cleanup();
        lock_map.erase(fd);
    }

    // perform the actual close operation on the original file
    return originalClose(fd);
}
