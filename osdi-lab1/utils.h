#ifndef UTILS_H
#define UTILS_H

int str_eq(const char *a, const char *b);

unsigned long read_cntfrq(void);
unsigned long read_cntpct(void);
void print_udec(unsigned long val, void (*putc_fn)(char));
void reset(int tick);

#endif