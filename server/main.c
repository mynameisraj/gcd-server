//
//  main.c
//  server
//
//  Created by Raj Ramamurthy on 8/20/14.
//  Copyright (c) 2014 Raj Ramamurthy. All rights reserved.
//

#include "server.h"
#include <unistd.h>

#define DEBUG_WEB_DIR "/Users/raj/dev/server/server/web"
    // When running in debug, read files from here.
    // Otherwise, use 'web' directory from where server is run.

int main(int argc, char*argv[]) {
    if (argc < 1) {
        printf("ERROR, no port provided\n");
        exit(1);
    }
    char* cwd = calloc(1024*sizeof(char), 1);
#ifdef DEBUG
    strcpy(cwd, DEBUG_WEB_DIR);
#else
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        request_destroy(req);
        response_destroy(res);
        perror("ERROR in getcwd");
    }
#endif
    unsigned int portno = atoi(argv[1]);
    server_t server;
    server.portno = portno;
    server.directory = cwd;
    server_start(server);
    return 0;
}
