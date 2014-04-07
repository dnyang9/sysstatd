

void *process(void *arg);

// static void echo(struct thread_args *args);

void handle_request(int fd, char *root);
void loadavg(int fd, char *valuei, char *version);
void meminfo(int fd, char *value, char *version);
void runloop(int fd, char *version);
void allocanon(int fd, char *version);
void freeanon(int fd, char *version);
void serve_file(int fd, char *root, char *path, char *version);
void filetype(char *path, char *type);
void send_response_header(int fd, char *version, char *filename, int filesize);
void clienterror(int fd, char *cause, char *version, char *errnum, 
		                             char *shortmsg, char *longmsg);
// static int send_msg(int sockfd, const void *msg, int len, int flags);
