#include <ucontext.h>

typedef struct {
	char *start;
	char *end;
	long len;
	char perm[4];
	char name[200];
} Section;

int save_ckpt_img(void);
char mtcp_readhex (char *s, char **value);
void signal_handler(int signo); 
void dump_img(void);
void write_context_to_ckpt_header(ucontext_t *context, int context_len);
void get_memory_range_and_permission(char *line, Section *s);
void write_to_ckpt(const void *buffer, int context_len);
char *trim_space(char *s);
void process_mem_range(char *mr, Section *ms);
int get_number_of_lines(char *file);
void write_memory_section_to_ckpt(Section *ms);
