//
// Created by Robert.Redl on 27.04.20.
//

#include "protocol.h"
#include "support.h"
#include <cstdio>
#include <ext/stdio_filebuf.h>

/*
 * Open and if necessary create the protocol file. The file will be locked directly.
 */
Protocol::Protocol() {
    logger->debug("opening protocol file {}", settings->PROTOCOL_FILE);
    this->fd = open_and_set_perm(settings->PROTOCOL_FILE);
    int err = originalFlock(this->fd, LOCK_EX);
    if (err != 0) logger->error("unable to lock protocol file {}", settings->PROTOCOL_FILE);
}

/*
 * make sure, that the file is closed when the object is deleted.
 */
Protocol::~Protocol() {
    if (fd > 0) this->close();
}

/*
 * Close the file again. That will release the lock as well.
 */
void Protocol::close() {
    logger->debug("closing protocol file.");
    originalClose(this->fd);
    fd = -1;
}

/*
 * store the new file and our own PID in the file
 */
void Protocol::add(string& path) {
    logger->debug("adding {} to protocol for PID {}", path, getpid());
    // append to the end of the file
    lseek(this->fd, 0, SEEK_END);
    dprintf(this->fd, "%d|%s\n", getpid(), path.c_str());
}

/*
 * Remove old files from the protocol and delete them. This is done when all processes that
 * opened a file before are already closed.
 */
void Protocol::cleanup() {
    logger->debug("checking protocol for old files");

    // create a stream from the file descriptor
    lseek(this->fd, 0, SEEK_SET);
    __gnu_cxx::stdio_filebuf<char> buf(this->fd, std::ios::in);
    istream is(&buf);

    // read the file line by line and store the result in maps
    string line;
    pid_t pid;
    string filename;
    int pos;
    map<string, vector<pid_t>> file_pid_map;
    while (!is.eof()) {
        getline(is, line);
        pos = line.find('|');
        if (pos == string::npos) break;
        pid = stoi(line.substr(0, pos));
        filename = line.substr(pos+1, line.size());
        logger->debug("    -> pid={}, filename={}", pid, filename);
        if (file_pid_map.count(filename) == 0) {
            file_pid_map[filename] = vector<pid_t>();
        }
        // store the pid, if the process is still running
        if (getpgid(pid) > 0) {
            file_pid_map[filename].push_back(pid);
        } else {
            logger->debug("    -> pid={} is dead!", pid);
        }
    }

    // iterate over all files and delete those that do not have a running process anymore
    lseek(this->fd, 0, SEEK_SET);
    for (const auto& one_files_pids: file_pid_map) {
        // no pids related anymore, delete the file
        if (one_files_pids.second.empty()) {
            int err = remove(one_files_pids.first.c_str());
            if (err != 0) logger->warn("unable to delete {}", one_files_pids.first);
            else logger->debug("deleted {}", one_files_pids.first);
        } else {
            // write the filename back to the protocol because it is still in use.
            for (const auto& one_pid: one_files_pids.second) {
                dprintf(this->fd, "%d|%s\n", one_pid, one_files_pids.first.c_str());
            }
        }
    }

    // truncate the protocol file to the new length
    size_t file_pos = lseek(this->fd, 0, SEEK_CUR);
    size_t file_size = lseek(this->fd, 0, SEEK_END);
    if (file_pos < file_size) {
        logger->debug("shrinking protocol size from {} to {}", file_size, file_pos);
        ftruncate(this->fd, file_pos);
    }
}
