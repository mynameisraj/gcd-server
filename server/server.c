//
//  server.c
//  server
//
//  Created by Raj Ramamurthy on 8/20/14.
//  Copyright (c) 2014 Raj Ramamurthy. All rights reserved.
//

#include "server.h"
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <dispatch/dispatch.h>
#include <sys/stat.h>
#include <errno.h>

// Response code numbers
#define HTTP_NOT_FOUND       404
#define HTTP_OK              200
#define HTTP_NOT_IMPLEMENTED 501

// Used for reading in the header
#define READ_INITIAL_SIZE 1024
#define READ_ITERATE_BY   1024

typedef struct {
    char* first_line;
    char* host;
    char* accept_encoding;
    char* accept;
    char* user_agent;
    char* accept_language;
    char* dnt;
    char* connection;
} request_t;

typedef struct {
    int           status;
    char*         response_code_string;
    char          content_type[10];
    unsigned long content_length;
    char*         connection;
    char*         body;
} response_t;

#define CONTENT_TYPE_HTML  "text/html"
#define CONTENT_TYPE_CSS   "text/css"
#define CONTENT_TYPE_JPG   "image/jpeg"
#define CONTENT_TYPE_PNG   "image/png"
#define CONTENT_TYPE_PLAIN "text/plain"

#define HTTP_404_CONTENT "<html><head><title>404 Not Found</title></head>\
                          <body><h1>404 Not Found</h1>The requested resource \
                          could not be found</body></html>"

#define HTTP_501_CONTENT "<html><head><title>501 Not Implemented</title> \
                          </head><body><h1>501 Not Implemented</h1>The server \
                          either does not recognise the request method, or it \
                          lacks the ability to fulfill the request.</body> \
                          </html>"

#define HTTP_200_STRING "OK"
#define HTTP_404_STRING "Not Found"
#define HTTP_501_STRING "Not Implemented"

static void server_handle_new_client(server_t, int sock);
    // Handle a new client. Will run until the client closes the connection

static request_t* read_request_header(int sock);
    // Read in the entire request header. Must free the request

static response_t* create_response(server_t, request_t*);
    // Creates a response. Must free response_t

static int request_fill(char** field, char* label, char* buf);
    //  Populates field with value of label from buf.
    //  Assumes format of label: value\r\n

static void request_destroy(request_t*);
    // Frees all memory associated with a request

static void response_destroy(response_t*);
    // Frees all memory associated with a response

static char* get_filename_from_request(const char*);
    // Gets the filename from a request

static void error(char*msg) {
    perror(msg);
    exit(1);
}

void server_start(server_t server) {
    signal(SIGPIPE, SIG_IGN);
    int sockfd, newsockfd;
    unsigned int len;
    struct sockaddr_in serv_addr, cli_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket\n");
    }
    memset((char*) &serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(server.portno);
    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding\n");
    }
    listen(sockfd,10);
    printf("Started server on port %u\n", server.portno);
    len = sizeof(cli_addr);

    // Main server loop
    while (1) {
    accept_connection:
        if((newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &len)) < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                goto accept_connection;
            }
            error("ERROR on accept.\n");
        }
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            server_handle_new_client(server, newsockfd);
        });
    }
}

static void server_handle_new_client(server_t server, int sock) {
    int written_header;
    printf("Open # %d\n", sock);
    while (1) {
        request_t* req = read_request_header(sock);
        if (req == NULL) {
            // No header
            goto cleanup;
        }
        response_t* res = create_response(server, req);
        char* response_buf = calloc(MAX_BODY_LEN, 1);
        if (!response_buf) {
            error("ERROR Could not form response buffer\n");
            response_destroy(res);
            break;
        }
        written_header = sprintf(response_buf,
                          "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: %s\r\n\r\n",
                          res->status, res->response_code_string, res->content_type,
                          res->content_length, res->connection);
        memcpy(response_buf+written_header, res->body, res->content_length);
        send(sock, response_buf, written_header+res->content_length, SO_NOSIGPIPE);
        free(response_buf);
        if (strcmp(res->connection, "keep-alive") != 0) {
            close(sock);
            response_destroy(res);
            break;
        }
        response_destroy(res);
    }
cleanup:
    printf("Close # %d\n", sock);
    close(sock);
}

static request_t* read_request_header(int sock) {
    request_t* req = calloc(sizeof(request_t), 1);
    int curr_size = READ_INITIAL_SIZE;
    char* req_buf = calloc(READ_INITIAL_SIZE, 1);
    int index_count = 0;

    if (!req || !req_buf) {
        free(req);
        free(req_buf);
        return NULL;
    }

    while (strstr(req_buf, "\r\n\r\n") == NULL) {
        char buf[READ_ITERATE_BY];
        memset(buf, '\0', READ_ITERATE_BY);
        recv(sock, buf, READ_ITERATE_BY, 0);
        if (strlen(buf) == 0) {
            free(req_buf);
            free(req);
            return NULL;
        }
        strncpy(req_buf+index_count, buf, READ_ITERATE_BY);
        index_count += READ_ITERATE_BY;
        if (READ_ITERATE_BY*index_count > curr_size) {
            req_buf = realloc(req_buf, (curr_size <<= 1) + 1);
        }
    }

    char* read_until = strchr(req_buf, '\r');
    long first_line_len = read_until-req_buf;
    req->first_line = calloc(first_line_len+1, 1);
    strncpy(req->first_line, req_buf, first_line_len);

    int error;
    error = request_fill(&req->host,            "Host",            req_buf);
    error = request_fill(&req->accept_encoding, "Accept-Encoding", req_buf);
    error = request_fill(&req->accept,          "Accept",          req_buf);
    error = request_fill(&req->user_agent,      "User-Agent",      req_buf);
    error = request_fill(&req->accept_language, "Accept-Language", req_buf);
    error = request_fill(&req->dnt,             "DNT",             req_buf);
    error = request_fill(&req->connection,      "Connection",      req_buf);

    free(req_buf);
    if (error == -1) {
        request_destroy(req);
        return NULL;
    }

    return req;
}

static response_t* create_response(server_t server, request_t* req) {
    response_t* res = calloc(sizeof(response_t), 1);
    const char* response_code_string;
    const char* content_type;
    char* filename = get_filename_from_request(req->first_line);
    char operation[2048];
    res->connection = calloc(strlen(req->connection), 1);
    if (filename != NULL) {
        if (strcmp(filename, "/") == 0) {
            strcat(filename, "index.html");
        }

        unsigned long cwd_len = strlen(server.directory);
        char* web = calloc(strlen(filename)+cwd_len, 1);
        strncpy(web, server.directory, cwd_len);
        strcat(web, filename);
        FILE* file;
        sprintf(operation, "GET %s", filename);
        free(filename);
        if ((file = fopen(web, "rb")) != NULL) {
            res->status = HTTP_OK;
            response_code_string = HTTP_200_STRING;
            fseek(file, 0, SEEK_END);
            res->content_length = ftell(file);
            rewind(file);
            char* file_ext = strrchr(web, '.');
            if (file_ext != NULL) {
                file_ext = file_ext + 1;
                if (strcmp(file_ext, "html") == 0) {
                    content_type = CONTENT_TYPE_HTML;
                } else if (strcmp(file_ext, "css") == 0) {
                    content_type = CONTENT_TYPE_CSS;
                } else if (strcmp(file_ext, "jpg") == 0) {
                    content_type = CONTENT_TYPE_JPG;
                } else if (strcmp(file_ext, "png") == 0) {
                    content_type = CONTENT_TYPE_PNG;
                } else {
                    content_type = CONTENT_TYPE_PLAIN;
                }
            } else {
                content_type = CONTENT_TYPE_PLAIN;
            }
            res->body = (char*)malloc(sizeof(char)*res->content_length);
            if (!res->body) {
                request_destroy(req);
                free(web);
                return NULL;
            }
            fread(res->body, 1, res->content_length, file);
            fclose(file);
        } else {
            res->body = calloc(strlen(HTTP_404_CONTENT), 1);
            if (!res->body) {
                request_destroy(req);
                response_destroy(res);
                free(web);
                return NULL;
            }
            strcpy(res->body, HTTP_404_CONTENT);
            res->status = HTTP_NOT_FOUND;
            response_code_string = HTTP_404_STRING;
            content_type = CONTENT_TYPE_HTML;
            res->content_length = strlen(res->body);
        }
        free(web);
    } else {
        res->body = calloc(strlen(HTTP_501_CONTENT), 1);
        if (!res->body) {
            request_destroy(req);
            response_destroy(res);
            return NULL;
        }
        strcpy(res->body, HTTP_501_CONTENT);
        res->status = HTTP_NOT_IMPLEMENTED;
        response_code_string = HTTP_501_STRING;
        content_type = CONTENT_TYPE_HTML;
        free(filename);
        res->content_length = strlen(res->body);
    }
    res->response_code_string = calloc(strlen(response_code_string), 1);
    if (!res->response_code_string) {
        request_destroy(req);
        response_destroy(res);
        return NULL;
    }
    strcpy(res->response_code_string, response_code_string);
    strcpy(res->content_type, content_type);
    strcpy(res->connection, req->connection);
    request_destroy(req);
    printf("%s: %d %s\n", operation, res->status, res->response_code_string);
    return res;
}

static int request_fill(char** field, char* label, char* buf) {
    char* start = strstr(buf, label);
    if (start == NULL) return -1;
    start += strlen(label) + 2; // For ": "
    char* end = strchr(start, '\r');
    if (end == NULL) return -1;
    long len = end - start;
    *field = calloc(sizeof(char)*len, 1);
    if (!*field) {
        printf("ERROR Could not fill request struct\n");
        return -1;
    }
    strncpy(*field, start, len);
    return 0;
}

static void request_destroy(request_t* req) {
    free(req->host);
    free(req->accept_encoding);
    free(req->accept);
    free(req->user_agent);
    free(req->accept_language);
    free(req->dnt);
    free(req->connection);
    free(req);
}

static void response_destroy(response_t* res) {
    free(res->response_code_string);
    free(res->connection);
    free(res->body);
    free(res);
}

static char* get_filename_from_request(const char* first_line) {
    if (strncmp(first_line, "GET ", 4) != 0) {
        return NULL;
    }
    char* http = strstr(first_line, "HTTP/1.1");
    if (http == NULL) return NULL;
    long len = http - first_line - 5;

    char* filename = calloc(len + 1, 1);
    strncpy(filename, first_line + 4, len);
    filename[len] = '\0';

    // Prevent a directory attack
    if (strstr(filename, "..")) {
        free(filename);
        return NULL;
    }
    
    return filename;
}
