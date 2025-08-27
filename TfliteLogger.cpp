#include <stdio.h>

static FILE* logFile = NULL;

const char* accessTypeStr[] = {"INPUT", "OUTPUT"};

void logMemAccess(void* address, int type) {
    if (logFile == NULL) {
        logFile = fopen("memory_trace.txt", "w");
        if (logFile == NULL) {
            perror("Error opening log file");
            return;
        }
    }
    // 기록
    fprintf(logFile, "%s %p\n", accessTypeStr[type], address);
}