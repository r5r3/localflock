/*
 * The protocol is used to keep track of all created files. All new files are written to the protocol,
 * on startup, old files not used anymore are removed.
 */

#ifndef LOCALFLOCK_PROTOCOL_H
#define LOCALFLOCK_PROTOCOL_H

#include <string>
using namespace std;

class Protocol {
public:
    Protocol();
    ~Protocol();
    void close();
    void add(string& path);
    void cleanup();
private:
    int fd;
};


#endif //LOCALFLOCK_PROTOCOL_H
