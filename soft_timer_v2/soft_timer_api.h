#if !defined(SOFTTIMERAPI)
#define SOFTTIMERAPI

int st_start(int ms, int periodic, void (*cb)(void *), void *arg);
int st_cancel(int id);
#endif // SOFTTIMERAPI
