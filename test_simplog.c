#include <stdio.h>

#include "include/simplog.h"

int main(int argc, char const *argv[])
{
    simplog.writeLog(SIMPLOG_FATAL, "Test Fatal");
    simplog.writeLog(SIMPLOG_ERROR, "Test Error");
    simplog.writeLog(SIMPLOG_INFO, "Test Info");
    simplog.writeLog(SIMPLOG_WARN, "Test Warn");
    simplog.writeLog(SIMPLOG_DEBUG, "Test Debug");
    simplog.writeLog(SIMPLOG_VERBOSE, "Test Verbose");
    
    return 0;
}
