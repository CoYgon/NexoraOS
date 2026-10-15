#ifndef EMREOS_PKG_H
#define EMREOS_PKG_H

/* EmrePkg: freestanding, RAM-resident package database for EmreOS. */
void pkg_init(void);
/* Handles the text after `pkg `. Returns 1 if handled, 0 otherwise. */
int pkg_execute(const char *args, char *out, unsigned cap);
unsigned pkg_installed_count(void);
#endif
