/*
 * Collection of all information related to one lock.
 */

#include "lock_info.h"
#include "support.h"

LockInfo::LockInfo(int original_fd) {
    this->original_fd = original_fd;

    // find the original absolute path to the given file
    this->orignal_path = get_path_for_fd(original_fd);

    // construct the name used for the local lock file
    this->local_path = get_local_lock_path(this->orignal_path);

    // actually create and open the local file
    this->local_fd = open(this->local_path.c_str(), O_CREAT | O_RDWR);
    filesystem::permissions(this->local_path, filesystem::perms::all
                            & ~filesystem::perms::owner_exec
                            & ~filesystem::perms::group_exec
                            & ~filesystem::perms::others_exec);
}

/*
 * Cleanup, close and delete the created file
 */
LockInfo::~LockInfo() {
    this->cleanup();
}

/*
 * get a string representation of the lock information for logging and debugging.
 */
string LockInfo::str() {
    return fmt::format("Lock-Infomration:\n    -> orig. FD:   {0}\n    -> orig. path: {1}\n    -> local FD:   {2}\n    -> local path: {3}",
            this->original_fd,
            this->orignal_path,
            this->local_fd,
            this->local_path);
}

/*
 * if open, close the local file
 */
void LockInfo::cleanup() {
    if (this->local_fd > 0) {
        originalClose(this->local_fd);
    }
}