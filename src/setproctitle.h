void init_setproctitle(int argc, char **_argv[]);
void setenvironstr(const char *fmt, ...);
void setproctitle(const char *fmt, ...);
void setproctitle_plus(const char *fmt, ...);
char *getproctitle();