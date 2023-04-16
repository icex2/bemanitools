#include <time.h>

#include "util/datetime.h"
#include "util/log.h"

bool util_datetime_now_formated(char* buffer, size_t len)
{
    log_assert(buffer);
    
    time_t current;
    struct tm *local;
    
    current = time(NULL);
    local = localtime(&current);

    // Don't use : for time to adhere to ISO8601 because this makes this format
    // not usable in filenames, e.g. for logfiles
    if (strftime(buffer, len, "%Y-%m-%dT%H-%M-%SZ", local) == 0) {
        return false;
    }

    return true;
}