#ifndef RESTART_H
#define RESTART_H

#include <ucontext.h>

typedef struct {
	char *start;
	char *end;
	long len;
	char perm[4];
	char name[200];
} Section;

void restore_memory();
void get_context_from_header(ucontext_t *c, int fd);
void restore_memory_helper(int fd);
int get_permission(Section *s);
void read_context(int fd, ucontext_t *c);
void remove_current_stack();
Section get_stack_section();
char mtcp_readhex (char *s, char **value);
void process_mem_range(char *mr, Section *ms);
void get_memory_range_and_name(char *line, Section *s);
char *trim_space(char *s);
#endif
