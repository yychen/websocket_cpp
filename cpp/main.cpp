#include <stdio.h>
#include <map>
#include <string>
#include <openssl/md5.h>
#include "SocketServer.h"

#ifdef WIN32
#include <WinSock2.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

#define HOST    "127.0.0.1"
#define PORT    9999

bool g_die = false;
SocketServer *server = NULL;

#ifdef WIN32
BOOL WINAPI ConsoleHandler(DWORD e)
{
    switch (e)
    {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
        g_die = true;
        break;
    }

    return TRUE;
}
#else
void sighandler(int sig)
{
    switch (sig)
    {
    case SIGPIPE:
        server->reset();
        break;

    default:
        g_die = true;
        break;
    }
}
#endif

int main()
{
#ifdef WIN32
    if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE) == FALSE)
    {
        printf("Unable to install handler.\n");
        return -1;
    }
#else
    signal(SIGINT, sighandler);
    signal(SIGPIPE, sighandler);
#endif

    server = new SocketServer(HOST, PORT);
    server->start();
    server->send("This is a test!!!");

    char msg[512] = {0};
    for (int i = 0; i < 500; i++)
    {
        if (g_die)
            break;

#ifdef WIN32
        Sleep(1000);
        
        sprintf_s(msg, "%d", i);
#else
        usleep(1000000);
        snprintf(msg, sizeof(msg) - 1, "%d", i);
#endif
        server->send(msg);
    }
    server->close();

    if (server != NULL)
        delete server;
    printf("MAIN caught close\n");
    return 0;
}
