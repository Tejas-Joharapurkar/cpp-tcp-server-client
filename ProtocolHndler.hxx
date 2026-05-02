#pragma once
#include <cstring>
#include <unistd.h>
#include "Message.hxx"

class ProtocolHndler
{
    static const int BUFFER_SIZE = 4096;

    char buffer[BUFFER_SIZE];
    char *startPos;
    char *endPos;
    bool msgAvailable;
    int bytesAvailable;
    int msgLen;
    int FD;

public:
    ProtocolHndler(int fd);

    // blocking — jab tak ek complete message na mile
    int Read();

    // complete message ka pointer return karo
    char *getMessage();

    // message consume karo — startPos aage badhao
    void consumeMessage();

    // encode karke bhejo
    int Write(const char *data, int len);

private:
    void Parse();
    void AdjustBuffer();
};