#include <unistd.h>

/* insert a character c at position pos in str of length len*/
void insert(char *str, size_t len, char c, size_t pos);

/*parse meminfo and print output as json format*/
void meminfo_json(int fd, char *buffer);

/*parse meminfo and print output as json format*/
void loadavg_json(int fd, char *buffer);

/*parse given input*/
void parse_request(char *input, char *req, char *value);
