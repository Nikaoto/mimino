// fdwatch - utilities around poll

#include <sys/resource.h>

static int default_max_poll_nfds = 10000;

int
fdwatch_get_max_poll_nfds()
{
#ifdef RLIMIT_NOFILE
    struct rlimit rl;
    int status = getrlimit(RLIMIT_NOFILE, &rl);
    if (status != 0)
        return default_max_poll_nfds;

    // Attempt to raise the soft max fd limit
    int ret = rl.rlim_cur;
    if (rl.rlim_max > rl.rlim_cur)
        rl.rlim_cur = rl.rlim_max;

    if (setrlimit(RLIMIT_NOFILE, &rl) == 0)
        return rl.rlim_cur - 1;

    return ret;
#endif // RLIMIT_NOFILE

    return default_max_poll_nfds;
}
