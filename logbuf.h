#ifndef LOGBUF_H
#define LOGBUF_H
void log_push(const char *fmt, ...);
void log_dump_recent(int max_lines);
#endif
