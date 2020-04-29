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
    // use human readable file names
    if (settings.SHOW_NAMES) {
        suffix.assign(path, 1, string::npos);
        for (int i=0; i < suffix.size(); i++) {
            if (suffix[i] == '/') suffix[i] = '-';
        }
    // calculate a hash for the file name
    } else {
        // length of the hash
        unsigned int digest_len = gcry_md_get_algo_dlen(GCRY_MD_SHA1);
        unsigned char* digest = (unsigned char *)malloc(digest_len * sizeof(unsigned char));
        char* digest_encoded = (char *)malloc(digest_len*2+1 * sizeof(char));
        gcry_md_hash_buffer(GCRY_MD_SHA1, digest, path.c_str(), path.size());

        for (int i=0; i<digest_len; i++) {
            sprintf(&digest_encoded[i * 2], "%02x", digest[i]);
        }
        digest_encoded[digest_len*2] = 0;
        suffix = string(digest_encoded);
        free(digest);
        free(digest_encoded);
    }
    string result = fmt::format("{}/{}", settings.LOCKDIR, suffix);
    return result;
}

/*
 * Create a new file and set permission for everyone
 */
int open_and_set_perm(string &path) {
    int fd = open(path.c_str(), O_CREAT | O_RDWR);

    // the file may already have to correct permissions, but a different owner. In this case, don't try to update
    // the permissions.
    filesystem::perms required_perms = filesystem::perms::all
                                       & ~filesystem::perms::owner_exec
                                       & ~filesystem::perms::group_exec
                                       & ~filesystem::perms::others_exec;
    filesystem::perms actual_perms = filesystem::status(path).permissions();
    if (actual_perms != required_perms) {
        logger->debug("updating permissions of {} from {} to {}.", path, perms_to_str(actual_perms), perms_to_str(required_perms));
        try {
            filesystem::permissions(path, required_perms);
        } catch (const std::exception& e) {
            logger->warn("open_and_set_perm: error during permission update:");
            logger->warn(e.what());
        }
    }
    return fd;
}

/**
 * Create a string representation of the permission for usage in an error message
 */
string perms_to_str(filesystem::perms p)
{
    string result = "";
    if ((p & filesystem::perms::owner_read) != filesystem::perms::none) result += "r";
    else result += "-";
    if ((p & filesystem::perms::owner_write) != filesystem::perms::none) result += "w";
    else result += "-";
    if ((p & filesystem::perms::owner_exec) != filesystem::perms::none) result += "x";
    else result += "-";
    if ((p & filesystem::perms::group_read) != filesystem::perms::none) result += "r";
    else result += "-";
    if ((p & filesystem::perms::group_write) != filesystem::perms::none) result += "w";
    else result += "-";
    if ((p & filesystem::perms::group_exec) != filesystem::perms::none) result += "x";
    else result += "-";
    if ((p & filesystem::perms::others_read) != filesystem::perms::none) result += "r";
    else result += "-";
    if ((p & filesystem::perms::others_write) != filesystem::perms::none) result += "w";
    else result += "-";
    if ((p & filesystem::perms::others_exec) != filesystem::perms::none) result += "x";
    else result += "-";
    return result;
}
