#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include "csapp.h"

void insert(char *str, size_t len, char c, size_t pos)
{
    memmove(&str[pos+1], &str[pos], len - pos + 1);
    str[pos] = c;
}

void parse_request(char *input, char *request, char *value)
{
//     sscanf(input, "%[/a-z]%*[?a-z1-9&=]callback=%[a-z1-9_.]", request, value);
    char *rest, *token, *ptr = input;
    int callback = 0;
    char *files = "/files";

    if (strncasecmp(input, files, strlen(files)) == 0)
    {
        sprintf(request, "%s", input);
    }
    else
    {
        while((token = strtok_r(ptr, "?=&", &rest)))
        {
        if(strcasecmp(token, "/loadavg") == 0 || strcasecmp(token, "/meminfo") == 0
                || strcasecmp(token, "/runloop") == 0 
                || strcasecmp(token, "/allocanon") == 0 
                || strcasecmp(token, "/freeanon") == 0)
            sprintf(request, "%s", token);
        else if (strcasecmp(token, "callback") == 0)
            callback = 1;

        ptr = rest;

        if (callback)
        {
            token = strtok_r(ptr, "?=&", &rest);
            sprintf(value, "%s", token);
            break;
        }
        }
    }
}

void loadavg_json(int fd, char *buffer)
{
    char *output = malloc(sizeof(char) * MAXLINE);
    insert(output, strlen(output), '{', 0);
    
    FILE *fp = fdopen(fd, "r");
    char buf[256];

    if (fgets(buf, sizeof(buf), fp) != NULL)
    {
        char avg1[10], avg2[10], avg3[10];
        char threads[20];

        sscanf(buf, "%s %s %s %s", avg1, avg2, avg3, threads);
        
        char current_threads[10], total_threads[10];
        sscanf(threads, "%[1-9]/%[1-9]", current_threads, total_threads);
        
        strcat(output, "\"total_threads\": \"");
        strcat(output, total_threads);
        strcat(output, "\", \"loadavg\": [\"");
        strcat(output, avg1);
        strcat(output, "\", \"");
        strcat(output, avg2);
        strcat(output, "\", \"");
        strcat(output, avg3);
        strcat(output, "\"], \"");
        strcat(output, "running_threads\": \"");
        strcat(output, current_threads);
        strcat(output, "\"}");
    }

//     printf("%s\n", output);

    strcpy(buffer, output);
    fclose(fp);
//     return output;
}

void meminfo_json(int fd, char *buffer)
{
    char *out = malloc(sizeof(char) * MAXLINE);
    insert(out, strlen(out), '{', 0);
//     insert(output, strlen(output), '(', 0);
    FILE *fp = fdopen(fd, "r");
    char buf[256];
    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        //printf("%s\n", buf);

        char c1[50];
        long c2;
        //scan file
        sscanf(buf, "%s %lu", c1, &c2);

        //insert quotation mark
        insert(c1, strlen(c1), '\"', strlen(c1) - 1);

        //convert to string
        char intstr[128];
        sprintf(intstr, "%lu", c2);
        //build output
//         strcat(output, "\"");
//         strcat(output, c1);
//         strcat(output, " \"");
//         strcat(output, intstr);
//         strcat(output, "\", ");
        sprintf(out, "%s\"%s \"%s\", ", out, c1, intstr);
    }

    out[strlen(out) - 2] = 0;
    insert(out, strlen(out), '}', strlen(out));
   
    strcpy(buffer, out);
    fclose(fp);
//     return buffer;
}

// int main(void)
// {
//     int fd;
//     fd = open("/proc/meminfo", O_RDONLY);
// 
//     char buf[MAXBUF];
//     meminfo_json(fd, buf);
// 
//     printf("%s\n", buf);
//     free(buf);
//     close(fd);
//     char *cmd = "/loadavg?callback=json123123123123123&_=asdfasdf123123123";
//     char *cmd = "/loadavg?callback=json1234123124";
//     char dest[100] = {0};
//     char value[100] = {0};
//     
//     int fd2;
//     fd2 = open("/proc/loadavg", O_RDONLY);
// 
//     char output[MAXBUF];
//     loadavg_json(fd2, output);
// 
//     printf("%s\n", output);
//     free(output);
//     close(fd2);
// 
//     printf("dest: %s\n", dest);
//     if (strlen(value) > 0)
//     printf("value: %s\n", value);
// 
//     return 0;
// }
