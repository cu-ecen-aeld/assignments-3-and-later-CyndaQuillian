#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT            9000
#define DATAFILE        "/var/tmp/aesdsocketdata"
#define RECV_CHUNK      4096
#define SEND_CHUNK      4096

static volatile sig_atomic_t g_exit_requested = 0;
static int g_server_fd = -1;

static void signal_handler(int sig)
{
    (void)sig;
    syslog(LOG_INFO, "Caught signal, exiting");
    g_exit_requested = 1;

    /* Wake up any blocking accept() */
    if (g_server_fd >= 0) {
        shutdown(g_server_fd, SHUT_RDWR);
    }
}

static int daemonise(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        perror("setsid");
        return -1;
    }

    int devnull = open("/dev/null", O_RDWR);
    if (devnull < 0) {
        perror("open /dev/null");
        return -1;
    }
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    chdir("/");

    return 0;
}

static int handle_client(int client_fd, const char *client_ip)
{
    size_t buf_size  = RECV_CHUNK;
    size_t buf_used  = 0;
    char  *buf       = malloc(buf_size);
    if (!buf) {
        syslog(LOG_ERR, "malloc failed: %s", strerror(errno));
        return -1;
    }

    int    packet_done = 0;
    ssize_t nr;

    while (!packet_done && !g_exit_requested) {
        if (buf_used + RECV_CHUNK > buf_size) {
            size_t new_size = buf_size * 2;
            char  *new_buf  = realloc(buf, new_size);
            if (!new_buf) {
                syslog(LOG_ERR, "realloc failed: %s", strerror(errno));
                free(buf);
                return -1;
            }
            buf      = new_buf;
            buf_size = new_size;
        }

        nr = recv(client_fd, buf + buf_used, RECV_CHUNK, 0);
        if (nr < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "recv: %s", strerror(errno));
            free(buf);
            return -1;
        }
        if (nr == 0) {
            break;
        }
        buf_used += (size_t)nr;
        if (memchr(buf, '\n', buf_used) != NULL) {
            packet_done = 1;
        }
    }

    if (buf_used == 0) {
        free(buf);
        return 0;
    }

    int data_fd = open(DATAFILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data_fd < 0) {
        syslog(LOG_ERR, "open " DATAFILE ": %s", strerror(errno));
        free(buf);
        return -1;
    }

    size_t written = 0;
    while (written < buf_used) {
        ssize_t nw = write(data_fd, buf + written, buf_used - written);
        if (nw < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "write to " DATAFILE ": %s", strerror(errno));
            close(data_fd);
            free(buf);
            return -1;
        }
        written += (size_t)nw;
    }
    close(data_fd);
    free(buf);

    data_fd = open(DATAFILE, O_RDONLY);
    if (data_fd < 0) {
        syslog(LOG_ERR, "open " DATAFILE " for read: %s", strerror(errno));
        return -1;
    }

    char sendbuf[SEND_CHUNK];
    ssize_t rd;
    while ((rd = read(data_fd, sendbuf, sizeof(sendbuf))) > 0) {
        ssize_t sent = 0;
        while (sent < rd) {
            ssize_t ns = send(client_fd, sendbuf + sent, (size_t)(rd - sent), 0);
            if (ns < 0) {
                if (errno == EINTR) continue;
                syslog(LOG_ERR, "send: %s", strerror(errno));
                close(data_fd);
                return -1;
            }
            sent += ns;
        }
    }
    if (rd < 0) {
        syslog(LOG_ERR, "read " DATAFILE ": %s", strerror(errno));
    }
    close(data_fd);

    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    return 0;
}

int main(int argc, char *argv[])
{
    int daemon_mode = 0;

    /* Parse arguments */
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
        case 'd':
            daemon_mode = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    if (sigaction(SIGINT,  &sa, NULL) < 0 ||
        sigaction(SIGTERM, &sa, NULL) < 0) {
        syslog(LOG_ERR, "sigaction: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    /* ---- Create server socket ---- */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        syslog(LOG_ERR, "socket: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    /* Allow rapid restart without TIME_WAIT issues */
    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        close(server_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "bind: %s", strerror(errno));
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) < 0) {
        syslog(LOG_ERR, "listen: %s", strerror(errno));
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (daemon_mode) {
        if (daemonise() < 0) {
            close(server_fd);
            closelog();
            return EXIT_FAILURE;
        }
    }

    g_server_fd = server_fd;

    while (!g_exit_requested) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            if (errno == EINTR || errno == EWOULDBLOCK) {
                continue;
            }
            if (g_exit_requested) break;
            syslog(LOG_ERR, "accept: %s", strerror(errno));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  client_ip, sizeof(client_ip));

        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        handle_client(client_fd, client_ip);

        close(client_fd);
    }

    close(server_fd);
    g_server_fd = -1;

    if (unlink(DATAFILE) < 0 && errno != ENOENT) {
        syslog(LOG_ERR, "unlink " DATAFILE ": %s", strerror(errno));
    }

    closelog();
    return EXIT_SUCCESS;
}
