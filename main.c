#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
#include<stdbool.h>
#include<dirent.h>
#include <sys/mman.h>
#include"structure.h"


#define MAXLINE 20000
#define MAXBUF 20000
#define LISTENQ 1024

char *home_path;

void doit(int fd);

void read_requesthdrs(rio_t *rp);

void serve_static(int fd, char *filename, int filesize, bool is_directory, struct stat sbuf);

void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);

void get_filetype(char *filename, char *filetype);

int parse_uri(char *uri, char *filename, char *cgiargs);

char *str_replace(char *orig, char *rep, char *with);

void create_html_code(char *filename, char *output);

int open_listenfd(int port);

void serve_dynamic(int fd, char *filename, char *cgiargs) {
    return;
}

int main(int argc, char **argv) {
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    home_path = strdup("/home");

    if (argc <= 1) {
        fprintf(stderr, "usage: %s <port> <folder>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);


    if(argc == 3){
    home_path = argv[2];
    }

    listenfd = open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (SA *) &clientaddr, &clientlen);
        doit(connfd);
        close(connfd);
    }
}

void doit(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented",
                    "WebServer ProPlusMax does not implement this method");
        return;
    }
    read_requesthdrs(&rio);

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found",
                    "WebServer ProPlusMax couldn't find this file");
        return;
    }

    if (is_static) {
        bool is_directory = (S_ISDIR(sbuf.st_mode));


        serve_static(fd, filename, sbuf.st_size, is_directory, sbuf);
    } else {/* Serve dynamic content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                        "WebServer ProPlusMax couldn't run the CGI program"
            );
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

void serve_static(int fd, char *filename, int filesize, bool is_directory, struct stat sbuf) {
    int srcfd;
    char *filetype = (char *) calloc(sizeof(char), 1000);
    char *buf = (char *) calloc(sizeof(char), 20000);

    if (is_directory) {


        char *output = (char *) calloc(sizeof(char), 200000);
        create_html_code(filename, output);

        filesize = strlen(output);
        sprintf(buf, "HTTP/1.0 200 OK\r\n");
        sprintf(buf, "%sServer: WebServer ProPlusMax\r\n", buf);
        sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
        sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, "text/html; charset=utf-8");

        rio_writen(fd, buf, strlen(buf));
        rio_writen(fd, output, filesize);
        free(output);
    } else {
        get_filetype(filename, filetype);
        sprintf(buf, "HTTP/1.0 200 OK\r\n");
        sprintf(buf, "%sServer: My WebServer\r\n", buf);
        sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
        sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
        rio_writen(fd, buf, strlen(buf));

        srcfd = open(filename, O_RDONLY, 0);
        char *srcp;
         srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
         close(srcfd);
         rio_writen(fd, srcp, filesize);
         munmap(srcp, filesize);

    }
    free(filetype);
    free(buf);
}

void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];
    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

int open_listenfd(int port) {
    int listenfd, optval = 1;

    struct sockaddr_in serveraddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *) &optval, sizeof(int)) < 0)
        return -1;

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) port);
    if (bind(listenfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
}

void create_html_code(char *filename, char *output) {
    char *temp = (char *) calloc(sizeof(char), 20000);
    struct stat sbuf;
    DIR *dirp = opendir(filename);
    if (dirp == NULL) {
        printf("Error: Cannot open dir\n");
        exit(2);
    }

    strcpy(output, "<!DOCTYPE html><html><head><title>WebServer ProPlusMax</title></head><div class=\"container\"><center><h1>WebServer ProPlusMax</h1></center></div><style>\n"
                   ".container {\n"
                   "  background: #e0e0e0;}</style>");
    strcat(output, "<head>Path: ");
    strcat(output, filename);
    strcat(output,
           "</head><body bgcolor=\"blanchedalmond\"><table width=\"100%\"><tr bgcolor=rgba(230,40,54,0.3)><th>Name</th><th>Size</th><th>Date</th></tr>");

    struct dirent *direntp;

    while ((direntp = readdir(dirp)) != NULL) {
        if (strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0)
            continue;
        strcpy(temp, filename);
        strcat(temp, "/");
        strcat(temp, direntp->d_name);

        if (stat(temp, &sbuf) < 0)
            continue;

        strcat(output, "<tr><td><a href=\"");
        strcat(output, filename);


        struct stat time;

        char *actual_filename = calloc(500, sizeof(char));
        strcpy(actual_filename, filename);
        strcat(actual_filename, "/");
        strcat(actual_filename, direntp->d_name);
        stat(actual_filename, &time);
        char *temp_time = calloc(100, sizeof(char));
        strcpy(temp_time, ctime(&time.st_mtime));

        sprintf(output, "%s/%s\">%s</a></td><td>%ld</td><td>%s</td></tr>", output, direntp->d_name, direntp->d_name,
                sbuf.st_size, temp_time);

        free(temp_time);
    }

    strcat(output, "</table></body></html>");

    closedir(dirp);
    free(temp);

}


//FIX COMMENTS AND CODE
char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;
    char *temp_uri = str_replace(uri, strdup("%20"), strdup(" "));
    if (temp_uri == NULL) {
        temp_uri = uri;
    }
    if (!strstr(temp_uri, "cgi-bin")) {
        strcpy(cgiargs, "");
        strcpy(filename, temp_uri);
        if (uri[strlen(temp_uri) - 1] == '/')
            if (strlen(temp_uri) <= 1) {
                strcpy(filename, home_path);
            }
        free(temp_uri);
        return 1;
    } else {
        ptr = index(temp_uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        } else
            strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, temp_uri);
        free(temp_uri);
        return 0;
    }
}


// SWITCH CASE
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".zip"))
        strcpy(filetype, "application/zip");
    else if (strstr(filename, ".pdf"))
        strcpy(filetype, "application/pdf");

    else
        strcpy(filetype, "text/plain");
}

void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg) {

    char *buf = (char *) calloc(sizeof(char), MAXLINE);
    char *body = (char *) calloc(sizeof(char), MAXBUF);

    sprintf(body, "<html><title>WebServer ProPlusMax Error</title>");
    sprintf(body, "%s<body bgcolor=\"blanchedalmond\"\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>WebServer ProPlusMax</em>\r\n", body);

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html; charset=utf-8\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int) strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
    free(buf);
    free(body);
}
