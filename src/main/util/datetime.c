#include <time.h>

#include "util/datetime.h"
#include "util/log.h"

bool util_datetime_now_iso_8601_formated(char* buffer, size_t len)
{
    log_assert(buffer);
    
    time_t current;
    struct tm *local;
    
    current = time(NULL);
    local = localtime(&current);

    if (strftime(buffer, len, "%Y-%m-%dT%H:%M:%SZ", local) == 0) {
        return false;
    }

    return true;
}