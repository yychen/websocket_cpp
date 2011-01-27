#ifndef __SOCKET_SERVER_H__
#define __SOCKET_SERVER_H__
#include <string>

#ifdef WIN32
#include <WinSock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <map>

typedef std::map<std::string, std::string> _HEADERS;

class SocketServer
{
public:
    SocketServer(const std::string &addr, const unsigned int port);
    ~SocketServer(void);

    void start();
    void send(const char *p, size_t n);
    void send(const std::string &msg);
    void waiting();
    void close();

#ifndef WIN32
    void reset();
#endif

private:
    
    void handshake();
    void parse_headers(const char *s, size_t n, _HEADERS &headers, std::string &key3, std::string &path);
    unsigned int extract_key(const char *key);

    inline void endian_swap(unsigned int& x)
    {
        x = (x>>24) | 
            ((x<<8) & 0x00FF0000) |
            ((x>>8) & 0x0000FF00) |
            (x<<24);
    };

    std::string m_addr;
    unsigned int m_port;
    bool m_init;
    bool m_ready;
    bool m_die;
#ifdef WIN32
    SOCKET m_listen_socket;
    SOCKET m_client_socket;
    HANDLE m_thread;
#else
    int m_listen_socket;
    int m_client_socket;
    pthread_t m_thread;
#endif
};

#endif
