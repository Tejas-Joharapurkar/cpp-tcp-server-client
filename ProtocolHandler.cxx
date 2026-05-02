#include "ProtocolHndler.hxx"
#include <iostream>
ProtocolHndler::ProtocolHndler(int fd) : FD(fd)
{
    startPos = buffer;
    endPos = buffer;
    msgAvailable = false;
    bytesAvailable = BUFFER_SIZE;
    msgLen = 0;
}

int ProtocolHndler::Read()
{
    while (!msgAvailable)
    {
        AdjustBuffer();

        int len = read(FD, endPos, bytesAvailable);

        if (len == 0)
            return 0; // client disconnected
        if (len < 0)
            return -1; // error

        endPos += len;
        bytesAvailable -= len;

        Parse(); // check if full message available
    }
    return 1;
}

void ProtocolHndler::Parse()
{
    int bytesRead = endPos - startPos;

    // Step 1: header parse and extract bodylen from it (bodylen does not contain header size)
    if (!msgLen && bytesRead >= (int)sizeof(Header))
    {
        const Header *hdr = reinterpret_cast<const Header *>(startPos);
        msgLen = hdr->bodyLen;
    }

    // Step 2: msgLen available check if i have data equal to sizeof(Header) + msgLen
    int totalLen = sizeof(Header) + msgLen;
    if (msgLen && bytesRead >= totalLen)
    {
        msgAvailable = true;
    }
    else
    {
        std::cout << "incomplete body " << FD << std::endl;
    }
}

char *ProtocolHndler::getMessage()
{
    if (!msgAvailable)
        return nullptr;
    return startPos;
}

void ProtocolHndler::consumeMessage()
{
    startPos += sizeof(Header) + msgLen;
    msgAvailable = false;
    msgLen = 0;

    // consume ke baad aur messages ho sakte hain buffer mein
    Parse();

    // space check
    AdjustBuffer();
}

int ProtocolHndler::Write(const char *data, int len)
{
    int totalSent = 0;
    while (totalSent < len)
    {
        int sent = write(FD, data + totalSent, len - totalSent);
        if (sent <= 0)
            return -1;
        totalSent += sent;
    }
    return totalSent;
}

void ProtocolHndler::AdjustBuffer()
{
    int dataLen = endPos - startPos;
    int spaceLeft = bytesAvailable;

    if (spaceLeft < 512)
    {
        memmove(buffer, startPos, dataLen);
        startPos = buffer;
        endPos = buffer + dataLen;
        bytesAvailable = BUFFER_SIZE - dataLen;
    }
}