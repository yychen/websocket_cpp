#include "SocketServer.h"
#include <openssl/md5.h>
#include <iostream>
#include <sstream>

#ifndef WIN32
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SOCKET_ERROR    (-1)
#define INVALID_SOCKET  (-1)
#endif

#ifdef WIN32
static DWORD WINAPI ThreadFunc(LPVOID obj)
#else
static void *ThreadFunc(void *obj)
#endif
{
    SocketServer *server = static_cast<SocketServer*>(obj);
    server->waiting();

    return 0;
}

SocketServer::SocketServer(const std::string &addr, const unsigned int port) :
    m_addr(addr), m_port(port), m_init(false), m_ready(false), m_client_socket(SOCKET_ERROR),
    m_listen_socket(INVALID_SOCKET), m_thread(NULL), m_die(false)
{
#ifdef WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    int wsaerr;

    wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);

    if (wsaerr != 0)
    {
        printf("This winsock dll not found\n");
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        printf("The dll doesn't support the Winsock version %u.%u!\n", LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion));
        WSACleanup();
    }
    else
    {
        m_init = true;
    }
#else
    m_init = true;
#endif
}


SocketServer::~SocketServer(void)
{
    if (m_thread)
    {
#ifdef WIN32
        CloseHandle(m_thread);
#endif
        m_thread = NULL;
    }

    if (m_ready)
    {
#ifdef WIN32
        closesocket(m_listen_socket);
#else
        ::close(m_listen_socket);
#endif
    }

#ifdef WIN32
    if (m_init)
        WSACleanup();
#endif
}

void SocketServer::start()
{
    if (!m_init)
        return;

    struct sockaddr_in service;

#ifdef WIN32
    m_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (m_listen_socket == INVALID_SOCKET)
    {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        return;
    }

    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr(m_addr.c_str());
    service.sin_port = htons(m_port);

    if (bind(m_listen_socket, (SOCKADDR *)&service, sizeof(service)) == SOCKET_ERROR)
    {
        printf("bind() failed: %ld.\n", WSAGetLastError());
        closesocket(m_listen_socket);
        return;
    }

    if (listen(m_listen_socket, 1) == SOCKET_ERROR)
    {
        printf("listen() failed: %ld.\n", WSAGetLastError());
        closesocket(m_listen_socket);
        return;
    }

    DWORD threadId;
    m_thread = CreateThread(0, 0, ThreadFunc, this, 0, &threadId);

#else

    m_listen_socket = socket(PF_INET, SOCK_STREAM, 0);

    if (m_listen_socket == -1)
    {
        printf("Error at socket(): %d\n", errno);
    }

    bzero(&service, sizeof(service));
    service.sin_family = PF_INET;
    service.sin_addr.s_addr = inet_addr(m_addr.c_str());
    service.sin_port = htons(m_port);

    if (bind(m_listen_socket, (struct sockaddr *)&service, sizeof(service)) == -1)
    {
        printf("bind() failed: %d.\n", errno);
        ::close(m_listen_socket);
        return;
    }

    if (listen(m_listen_socket, 1) == -1)
    {
        printf("listen() failed: %d.\n", errno);
        ::close(m_listen_socket);
        return;
    }

    pthread_create(&m_thread, NULL, ThreadFunc, this);
#endif
}

void SocketServer::close()
{
    m_ready = false;
    m_die = true;

    printf("Waiting for server to close...\n");

#ifdef WIN32
    closesocket(m_listen_socket);
    if (m_thread != NULL)
    {
        WaitForSingleObject(m_thread, INFINITE);
    }
#else
    ::close(m_listen_socket);
#endif
    printf("closed...\n");
}

void SocketServer::waiting()
{
    m_ready = false;
    while (!m_die)
    {
        while (m_client_socket == SOCKET_ERROR && !m_die)
        {
            printf("Accepting...\n");
            m_client_socket = accept(m_listen_socket, NULL, NULL);
            printf("inside\n");
        }

        printf("Outside\n");

        if (m_client_socket != SOCKET_ERROR)
        {
            printf("Server: Client connected\n");
            handshake();

            while(m_ready)
            {
#ifdef WIN32
                Sleep(100);
#else
                usleep(100000);
#endif
            }

            m_client_socket = SOCKET_ERROR;
        }
    }
}

void SocketServer::parse_headers(const char *s, size_t n, _HEADERS &headers, std::string &key3, std::string &path)
{
    char _buf[512] = {0};

    memcpy(_buf, s, n);
    char *start = _buf, *curr = _buf;
    std::string key, value;
    enum _status { STATUS_HTTPSTATUS, STATUS_KEY, STATUS_VALUE, STATUS_CONTENT };
    _status status = STATUS_HTTPSTATUS;

    bool flag = false;
    for (size_t i = 0; i < n; i++)
    {
        if ((status == STATUS_HTTPSTATUS || status == STATUS_VALUE) && (*curr == '\r' && *(curr + 1) == '\n'))
        {
            
            *curr = '\0';

            value = start;
            
            if (status == STATUS_HTTPSTATUS)
            {
                /* Extract path */
                char *p = start + 4, *tmpcurr = p;

                do
                {
                    /* not found */
                    if (*tmpcurr == '\r' || *tmpcurr == '\n')
                        break;

                    if (*tmpcurr == ' ')
                    {
                        *tmpcurr = '\0';
                        path = p;
                        break;
                    }
                } while(*tmpcurr++);
                // printf("STATUS: %s, path: %s\n", value.c_str(), path.c_str());
            }
            else
            {
                printf("KEYVALUE ==> %s: %s\n", key.c_str(), value.c_str());

                headers.insert(_HEADERS::value_type(key, value));
            }
            
            /* Content */
            if (*(curr + 2) == '\r' && *(curr + 3) == '\n')
            {
                curr += 4;
                key3 = curr;
                // printf("LAST 8: %s (%d)\n", key3.c_str(), key3.size());
                break;
            }
            curr += 2;
            start = curr;

            status = STATUS_KEY;
        }
        else if (status == STATUS_KEY && *curr == ':')
        {
            *curr = '\0';
            key = start;

            curr += 2;
            start = curr;

            status = STATUS_VALUE;
        }
        else
        {
            curr++;
        }
    }
}

unsigned int SocketServer::extract_key(const char *key)
{
    char keytmp[64] = {0}, *p = keytmp;
    int spaces = 0;

    if (key == NULL)
        return 0;

    do
    {
        if (*key >= '0' && *key <= '9')
            *p++ = *key;

        if (*key == ' ')
            spaces++;
    } while (*key++);

    unsigned int intkey = strtoul(keytmp, NULL, 10);

    return (intkey / spaces);
}

void SocketServer::handshake()
{
    char buf[512] = {0};
    int bytes = 0;
    _HEADERS headers;
    unsigned int key1 = 0, key2 = 0;
    std::string key3, path;

    /* Receive the headers */
    bytes = recv(m_client_socket, (char *)buf, sizeof(buf), 0);
    if (bytes == SOCKET_ERROR)
    {
#ifdef WIN32
        printf("recv() error: %ld.\n", WSAGetLastError());
#else
        printf("recv() error: %d.\n", errno);
#endif
        return;
    }

    /* Extract the keys */
    parse_headers(buf, bytes, headers, key3, path);
    key1 = extract_key(headers["Sec-WebSocket-Key1"].c_str());
    key2 = extract_key(headers["Sec-WebSocket-Key2"].c_str());

    /* Compose the challenge */
    unsigned char challenge[16] = {0};
    endian_swap(key1);
    endian_swap(key2);
    memcpy(challenge + 0, &key1, 4);
    memcpy(challenge + 4, &key2, 4);
    memcpy(challenge + 8, key3.c_str(), 8);

    unsigned char generated[16] = {0};
    MD5(challenge, 16, generated);

#ifdef WIN32
    sprintf_s(buf, "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"\
            "Upgrade: WebSocket\r\nConnection: Upgrade\r\n"\
            "Sec-WebSocket-Origin: %s\r\n"\
            "Sec-WebSocket-Location: ws://%s%s\r\n\r\n", headers["Origin"].c_str(), headers["Host"].c_str(), path.c_str());
#else
    snprintf(buf, sizeof(buf) - 1, "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"\
            "Upgrade: WebSocket\r\nConnection: Upgrade\r\n"\
            "Sec-WebSocket-Origin: %s\r\n"\
            "Sec-WebSocket-Location: ws://%s%s\r\n\r\n", headers["Origin"].c_str(), headers["Host"].c_str(), path.c_str());
#endif

    bytes = ::send(m_client_socket, buf, strlen(buf), 0);
    // printf("Sending: \n%s\n", buf);
    bytes = ::send(m_client_socket, (char *)generated, 16, 0);

    printf("Handshake passed...");
    m_ready = true;
}

void SocketServer::send(const char *p, size_t n)
{
    std::stringstream stream;
    size_t bytes = 0;

    if (!m_ready)
        return;

    printf("Sending: %s\n", p);
    stream << '\x00' << p << '\xFF';
    bytes = ::send(m_client_socket, stream.str().c_str(), n + 2, 0);

    if (bytes == SOCKET_ERROR)
    {
        char buf[256] = {0};

        // FormatMessage(0, 0, WSAGetLastError(), 0, buf, sizeof(buf), 0);
#ifdef WIN32
        printf("SOCKET_ERROR: %d\n", WSAGetLastError());
#else
        printf("SOCKET_ERROR: %d\n", errno);
#endif
        m_ready = false;
    }
}

#ifndef WIN32
void SocketServer::reset()
{
    m_ready = false;
}
#endif

void SocketServer::send(const std::string &msg)
{
    send(msg.c_str(), msg.size());
}
