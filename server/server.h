//
//  server.h
//  server
//
//  Created by Raj Ramamurthy on 8/20/14.
//  Copyright (c) 2014 Raj Ramamurthy. All rights reserved.
//

#ifndef server_server_h
#define server_server_h

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

typedef struct {
    unsigned int portno;
    char*        directory;
} server_t;

#define MAX_BODY_LEN (10*1024*1024)
    // Maximum request size (10MB)

void server_start(server_t);
    // Start

#endif
