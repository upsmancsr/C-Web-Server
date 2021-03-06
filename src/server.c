/**
 * webserver.c -- A webserver written in C
 * 
 * Test with curl (if you don't have it, install it):
 * 
 *    curl -D - http://localhost:3490/
 *    curl -D - http://localhost:3490/d20
 *    curl -D - http://localhost:3490/date
 * 
 * You can also test the above URLs in your browser! They should work!
 * 
 * Posting Data:
 * 
 *    curl -D - -X POST -H 'Content-Type: text/plain' -d 'Hello, sample data!' http://localhost:3490/save
 * 
 * (Posting data is harder to test from a browser.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"

#define PORT "3490"  // the port users will be connecting to

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"

/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 * 
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body, int content_length)
{
    const int max_response_size = 200000;  // Originally 65536; cat.jpg is ~170k
    char response[max_response_size];      // *NOTE that the variable name `response` used later will resolve to the address of the first element (&response[0])

    // Build HTTP response and store it in response
    // IMPLEMENT ME! //
    time_t time_response_sent = time(NULL);  // create a marker for time
    
    // Define response_length:
    int response_length = sprintf(
        response,
        "%s\nDate: %sConnection: close\nContent-Length: %d\nContent-Type: %s\n\n",    
        header, 
        asctime(localtime(&time_response_sent)), 
        content_length, 
        content_type
    );
    // printf("response pointer: %i\n", *response);
    // printf("response_length: %i\n", response_length);
    // printf("response + response_length: %i\n", *response + response_length);
    // printf("body pointer: %i\n", body);


    memcpy(response + response_length, body, content_length);   // memcpy(void *a, const void *b, size_t n)
                                                                // copies n characters from a memory area pointed to by b to a memory area pointed to by a
    response_length += content_length;                          // get new response_length

    // Send it all!
    int rv = send(fd, response, response_length, 0);
    if (rv < 0) {
        perror("send");
    }
    return rv;
}

/**
 * Send a /d20 endpoint response
 */
void get_d20(int fd)
{
    printf("\nget_d20() called");
    // Generate a random number between 1 and 20 inclusive
    // IMPLEMENT ME! //

    srand(time(NULL));                           // initialize random generator
    int random_num = (rand() % 20) + 1;          // generate a random number b/w 1 and 20 inc.
    printf("\nrandom_num == %d", random_num);
    char body_content[8];                        // initialize an 8-bit char
    sprintf(body_content, "%d\n", random_num);   // sprintf the random number into body_content
    printf("\nresponse body_content == %s", body_content);

    // char header[] = "HTTP/1.1 200 OK";           // define the header for the response
    // char content_type[] = "text/plain";          // define the content_type for the response body

    // Use send_response() to send it back as text/plain data
    // IMPLEMENT ME! //
    send_response(fd, "HTTP/1.1 200 OK", "text/plain", body_content, strlen(body_content));
}

/**
 * Send a 404 response
 */
void resp_404(int fd)
{
    char filepath[4096];
    struct file_data *filedata; 
    char *mime_type;

    // Fetch the 404.html file
    snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
    filedata = file_load(filepath);

    if (filedata == NULL) {
        // TODO: make this non-fatal
        fprintf(stderr, "cannot find system 404 file\n");
        exit(3);
    }

    mime_type = mime_type_get(filepath);

    // int send_response(int fd, char *header, char *content_type, void *body, int content_length)
    send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);

    file_free(filedata);
}

/**
 * Read and return a file from disk or cache
 */
// request_path should map to file name in serverroot directory
// use methods in file.c to load/read the corresponding file
// use methods in mime.c to set the content-type header based on the type of data in the file
void get_file(int fd, struct cache *cache, char *request_path)
{
    // IMPLEMENT ME! //
    printf("\nget_file() called");

    struct file_data *filedata = NULL;       // declare a pointer to a file_data struct
    char *mime_type;                         // declare a pointer to a mime_type aka content_type
    char file_path[4096];                    // declare an array to store the file path

    // Use requet_path for the cache; use full file_path (./serverroot + request_path) for loading files not in the cache
    struct cache_entry *entry = cache_get(cache, request_path);   // get the desired entry from the cache (if it's there)

    if (entry == NULL) {    // if cache has no entry with the given request_path

        sprintf(file_path, "./serverroot%s", request_path);   // build a full file path into the ./serverroot directory
        filedata = file_load(file_path);                      // returns filedata->data and filedata->size; *NOTE: file_load allocates memory that should be freed later
        
        // TO-DO: handle case where user inputs '/' as the path, and serve index.html file
         if (filedata == NULL) {
             sprintf(file_path, "./serverroot%s/index.html", request_path);
             filedata = file_load(file_path);
             if (filedata == NULL) {
                resp_404(fd);
                return;
            }
         }
            
        // else if filedata != NULL:
        mime_type = mime_type_get(file_path);  // checks the file extension and returns `content-type` string

        cache_put(cache, request_path, mime_type, filedata->data, filedata->size);  // put an entry in cache for the given file

        send_response(fd, "HTTP/1.1 200 OK", mime_type, filedata->data, filedata->size);
        file_free(filedata);
        return;
    }
    // else if entry != NULL:
    
    send_response(fd, "HTTP/1.1 200 OK", entry->content_type, entry->content, entry->content_length);
    return;
}

/**
 * Search for the end of the HTTP header
 * 
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *header)
{
    // IMPLEMENT ME! // (Stretch)
}

/**
 * Handle HTTP request and send response
 */
void handle_http_request(int fd, struct cache *cache)
{
    const int request_buffer_size = 65536; // 64K
    char request[request_buffer_size];

    // Declare and degine buffers for the request:
    char request_type[8]; // e.g. GET or POST
    char request_path[1024]; // URL path info, e.g., /d20
    char request_protocol[128]; // e.g. HTTP/1.1

    // Read request
    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);

    if (bytes_recvd < 0) {
        perror("recv");
        return;
    }

    // IMPLEMENT ME! //
    // Read the three components of the first request line
    sscanf(request, "%s %s %s", request_type, request_path, request_protocol);

    printf("\nHTTP request made -- \nType: %s \nPath: %s \nProtocol: %s\n", request_type, request_path, request_protocol);

    // If request_type is GET, handle the get endpoints:
    if(strcmp(request_type, "GET") == 0) {
        //  Check if request_path is /d20 and handle that special case:
        if(strcmp(request_path, "/d20") == 0) {
            get_d20(fd); 
        } else {
            // resp_404(fd);
            get_file(fd, cache, request_path);  // if path is not /d20, then get file at the specified path
        }

    } else {
        resp_404(fd); // if request_type is not GET, give 404 response
    }

    // (Stretch) If POST, handle the post request
}

/**
 * Main
 */
int main(void)
{
    int newfd;  // listen on sock_fd, new connection on newfd
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];

    struct cache *cache = cache_create(10, 0);

    // Get a listening socket
    int listenfd = get_listener_socket(PORT);

    if (listenfd < 0) {
        fprintf(stderr, "webserver: fatal error getting listening socket\n");
        exit(1);
    }

    printf("webserver: waiting for connections on port %s...\n", PORT);

    // This is the main loop that accepts incoming connections and
    // forks a handler process to take care of it. The main parent
    // process then goes back to waiting for new connections.
    
    while(1) {
        socklen_t sin_size = sizeof their_addr;

        // Parent process will block on the accept() call until someone
        // makes a new connection:
        newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);

        if (newfd == -1) {
            perror("accept");
            
            continue;
        }
        // resp_404(newfd);  // test send_response without any other new code


        // Print out a message that we got the connection
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        // newfd is a new socket descriptor for the new connection.
        // listenfd is still listening for new connections.        

        handle_http_request(newfd, cache);

        close(newfd);
    }

    // Unreachable code

    return 0;
}

