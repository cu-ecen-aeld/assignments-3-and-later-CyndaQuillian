#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    openlog("writer", LOG_PID, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Usage: %s <file path> <string to write>", argv[0]);
        closelog();
        return 1;
    }

    const char *file_path = argv[1];
    const char *write_str = argv[2];

    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        syslog(LOG_ERR, "Error opening file '%s': %s", file_path, strerror(errno));
        closelog();
        return 1;
    }

    if (fputs(write_str, fp) == EOF) {
        syslog(LOG_ERR, "Error writing to file '%s': %s", file_path, strerror(errno));
        fclose(fp);
        closelog();
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", write_str, file_path);

    fclose(fp);
    closelog();
    return 0;
}