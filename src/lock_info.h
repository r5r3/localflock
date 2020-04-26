/*
 * Collection of all information related to one lock.
 */

#ifndef LOCALFLOCK_LOCK_INFO_H
#define LOCALFLOCK_LOCK_INFO_H

#include <string>
#include <fcntl.h>
#include <fcntl.h>

using namespace std;

class LockInfo {
public:
    LockInfo(int original_fd);
    ~LockInfo();
    string str();
    void cleanup();
    int original_fd;
    string orignal_path;
    int local_fd;
    string local_path;
};

#endif //LOCALFLOCK_LOCK_INFO_H
