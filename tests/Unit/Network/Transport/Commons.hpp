
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
int pick_available_port()
{
    for (int port = 1024; port <= 65535; ++port)
    {
        int fd = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0)
            continue;

        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_any;
        addr.sin6_port = htons(static_cast<uint16_t>(port));

        bool ok =
            (::bind(fd,
                    reinterpret_cast<sockaddr*>(&addr),
                    sizeof(addr)) == 0);

        ::close(fd);

        if (ok)
            return port;
    }

    return -1;
}