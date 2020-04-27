/*
 * Collection of all information related to one lock.
 */

#include "lock_info.h"
#include "support.h"
#include "protocol.h"

LockInfo::LockInfo(int original_fd) {
    this->original_fd = original_fd;

    // find the original absolute path to the given file
    this->orignal_path = get_path_for_fd(original_fd);

    // construct the name used for the local lock file
    this->local_path = get_local_lock_path(this->orignal_path);

    // make a protocol of writing this file. This is used to cleanup later
    Protocol* proto = new Protocol();
    proto->add(this->local_path);

    // actually create and open the local file
    this->local_fd = open_and_set_perm(this->local_path);

    // close the protocol and release the lock
    proto->close();
    delete proto;
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
    return fmt::format("Lock-Information:\n    -> orig. FD: {0}, path: {1}\n    -> local FD: {2}, path: {3}",
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
        this->local_fd = -1;
    }
}