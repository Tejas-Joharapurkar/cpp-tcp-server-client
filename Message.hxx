#pragma once
#include <cstdint>
#include <cstring>

enum MessageType : uint16_t
{
    MSG_ORDER = 1,
    MSG_RESPONSE = 2,
};

#pragma pack(push, 1)
struct Header
{
    uint32_t bodyLen;
    uint32_t networkID;
    uint16_t templateID;
};

struct Order
{
    double price;
    uint32_t quantity;
    char symbol[8];
    uint32_t side;
};

struct PriceLevel
{
    double price;
    uint32_t qty;
};

struct OrderBook
{
    uint32_t count;
    PriceLevel levels[5];
};

struct Message
{
    Header header;
    Order order;
};

// string NAHI — fixed buffer use karo wire pe
struct Response
{
    Header header;
    uint32_t resLen;
    char res[256];
};
#pragma pack(pop)

int encode(const Message &message, char *buf, int bufSize);
int encode(const Response &message, char *buf, int bufSize);
int decode(const char *buf, Message &message);
int decode(const char *buf, Response &message);