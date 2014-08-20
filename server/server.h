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

#define DEBUG_WEB_DIR "/Users/raj/dev/server/server/web"
    // When running in debug, read files from here.
    // Otherwise, use 'web' directory from where server is run.

#define MAX_BODY_LEN (10*1024*1024)
    // Maximum request size (10MB)

void server_start_on(unsigned int portno);
    // Start on the given port

#endif
