//
//  main.c
//  server
//
//  Created by Raj Ramamurthy on 8/20/14.
//  Copyright (c) 2014 Raj Ramamurthy. All rights reserved.
//

#include "server.h"

int main(int argc, char*argv[]) {
    if (argc < 1) {
        printf("ERROR, no port provided\n");
        exit(1);
    }
    unsigned int portno = atoi(argv[1]);
    server_start_on(portno);
    return 0;
}
