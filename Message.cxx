#include "Message.hxx"
#include <cstring>
#include <iostream>

int encode(const Message &message, char *buf, int bufSize)
{
    int required = sizeof(Header) + sizeof(Order);
    if (bufSize < required)
    {
        std::cerr << "Buffer too small for Message\n";
        return -1;
    }

    char *ptr = buf;
    const Order &order = message.order;

    ptr += sizeof(Header); // header ke liye jagah chod
    char *bodyStart = ptr;

    memcpy(ptr, &order.price, sizeof(order.price));
    ptr += sizeof(order.price);
    memcpy(ptr, &order.quantity, sizeof(order.quantity));
    ptr += sizeof(order.quantity);
    memcpy(ptr, &order.symbol, sizeof(order.symbol));
    ptr += sizeof(order.symbol);
    memcpy(ptr, &order.side, sizeof(order.side));
    ptr += sizeof(order.side);

    Header final = message.header;
    final.bodyLen = ptr - bodyStart;
    final.templateID = MSG_ORDER;
    memcpy(buf, &final, sizeof(Header));

    return ptr - buf;
}

int encode(const Response &res, char *buf, int bufSize)
{
    uint32_t len = strnlen(res.res, sizeof(res.res));
    int required = sizeof(Header) + sizeof(uint32_t) + len;
    if (bufSize < required)
    {
        std::cerr << "Buffer too small for Response\n";
        return -1;
    }

    char *ptr = buf;
    ptr += sizeof(Header);
    char *bodyStart = ptr;

    memcpy(ptr, &len, sizeof(len));
    ptr += sizeof(len);
    memcpy(ptr, res.res, len);
    ptr += len;

    Header final = res.header;
    final.bodyLen = ptr - bodyStart;
    final.templateID = MSG_RESPONSE;
    memcpy(buf, &final, sizeof(Header));

    return ptr - buf;
}

int decode(const char *buf, Message &message)
{
    const char *ptr = buf;

    memcpy(&message.header, ptr, sizeof(Header));
    ptr += sizeof(Header);

    memcpy(&message.order.price, ptr, sizeof(message.order.price));
    ptr += sizeof(message.order.price);
    memcpy(&message.order.quantity, ptr, sizeof(message.order.quantity));
    ptr += sizeof(message.order.quantity);
    memcpy(&message.order.symbol, ptr, sizeof(message.order.symbol));
    ptr += sizeof(message.order.symbol);
    memcpy(&message.order.side, ptr, sizeof(message.order.side));
    ptr += sizeof(message.order.side);

    return ptr - buf;
}

int decode(const char *buf, Response &message)
{
    const char *ptr = buf;

    memcpy(&message.header, ptr, sizeof(Header));
    ptr += sizeof(Header);

    memcpy(&message.resLen, ptr, sizeof(message.resLen));
    ptr += sizeof(message.resLen);

    uint32_t safeLen = message.resLen < sizeof(message.res) ? message.resLen : sizeof(message.res) - 1;
    memcpy(message.res, ptr, safeLen);
    ptr += message.resLen;
    message.res[safeLen] = '\0';

    return ptr - buf;
}