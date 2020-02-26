/* 
    Joseph Fergen
    Proxy Server Client
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>

int main (int argc, char *argv[]) {
    // No port entered in arguments
    if (argc != 2) {
        fprintf(stderr, "Please provide a port numer to use \n");
        fprintf(stderr, "%s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int master_fd;                              // Master socket
    int max_fd, curr_fd, activity, i, bytes, total;
    int hours, minutes, seconds, day, month, year;
    int numCached = 0;
    //int maxClients = 2;                         // Max num of clients is two for client and HTTP requests
    //int numClients = 0;
    //int currClient = 0;                         // Boolean to see if there is currently a client connected (0 = false, 1 = true)
    int welcomeMsg = 0;
    struct sockaddr_in serv_addr, cli_addr, http_addr;
    fd_set readfds;                             // Used for selecting socket to use
    int cliLength;
    int httpSocket = 0;
    int httpPort = 80;                          // HTTP always port 80 for requests
    char httpHost[200];
    char httpSend[2000];                        // Holds the HTTP GET request
    int htmlStart;                              // Boolean that says then HTML code starts
    struct hostent* httpServer;                 // Holds the server information for the HTTP request
    int opt = 1;
    char responseFile[200];
    char fileName[200];

    FILE *fileptr;
    FILE *fileptr2;
    char line[100];

    time_t timeNow;

    // AF_INET - IPv4 IP , Type of socket, protocol
    if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Setup master socket to allow multiple connections
    if (setsockopt(master_fd, SOL_SOCKET, SO_REUSEADDR, (char*) &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Set up server information
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htons(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    // Binds the above details to the socket
    if (bind(master_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Start listening to incoming connections
    if (listen(master_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Listening on port %s\n", argv[1]);
    printf("Waiting for connections...\n");

    cliLength = sizeof(cli_addr);

    // Accepts incoming connection
    while(1) {
        char client_message[8192];  // Message from client server
        int foundSite = 0;          // Boolean to see if the site was found in list.txt

        // Clear the socket set
        FD_ZERO(&readfds);

        // Add master socket to set
        FD_SET(master_fd, &readfds);
        max_fd = master_fd;

        
        if (curr_fd > 0) {
            FD_SET(curr_fd, &readfds);
        }

        if (curr_fd > max_fd) {
            max_fd = curr_fd;
        }
        
        // Wait for activity on a socket
        if ((activity = select(max_fd + 1, &readfds, NULL, NULL, NULL)) < 0) {
            perror("select");
        }

        // If something happened on master socket, there is an incoming connection
        if (FD_ISSET(master_fd, &readfds)) {
            if ((curr_fd = accept(master_fd, (struct sockaddr*) &cli_addr, (socklen_t*) &cli_addr)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // TODO:: Make it so only 1 real client is allowed at once
            // if (numClients == maxClients) {
            //     char* kickMsg = "Only one client allowed at once"
            //     close(curr_fd);
            //     --curr_fd;
            //     continue;
            // }

            printf("New connection, Socket fd: %d, IP: %s, PORT: %d\n", curr_fd, inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);
            //++numClients;
            //currClient = 1;

            if (welcomeMsg == 0) {
                char *welcomeMsg = "Connected to server. Please enter a URL starting with \"www.\"";

                if (write(curr_fd, welcomeMsg, strlen(welcomeMsg)) < 0) {
                    perror("write");
                    exit(EXIT_FAILURE);
                }

                printf("Welcome message sent\n");
                welcomeMsg = 1;
            }
        }
        
        bzero(client_message, sizeof(client_message));

        if (FD_ISSET(curr_fd, &readfds)) {
            // Check if socket is closing and read message
            if (read(curr_fd, client_message, sizeof(client_message)) == 0) {   // Closing
                // Get information of closing socket
                if (getpeername(curr_fd, (struct sockaddr*) &cli_addr, (socklen_t*) &cliLength) < 0) {
                    perror("getpeername");
                }

                // Close socket and mark as open for next client
                close(curr_fd);
                //--numClients;
                //conn_fd[i] = 0;

                // If client disconnected
                //if (i == 0) {
                welcomeMsg = 0;
                //}

                // Print information of closing socket
                printf("Client disconnected. Socket fd: %d, IP: %s, PORT: %d\n", curr_fd, inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);
            } else {    // Not closing
                // Place null terminating character at end of message
                if (client_message[strlen(client_message) - 1] == '\n') {
                    client_message[strlen(client_message) - 1] = '\0';
                }

                // Set all of message to be lowercase
                for (i = 0; client_message[i] != '\0'; ++i) {
		            if (isupper(client_message[i])) {
			            client_message[i] = tolower(client_message[i]);
		            }
                }
    
                // If client enters "quit", stop the server
                if (strcmp(client_message, "quit") == 0) {
                    printf("Quitting...\n");
                    exit(EXIT_SUCCESS);
                }

                strcpy(httpHost, client_message);   // Save the host name

                /*-----Check list.txt for the input host-----*/

                // Open up list.txt for appending & reading
                fileptr = fopen("list.txt", "r");
                if (!fileptr) {
                    perror("fopen");
                } else {
                    while (fgets(line, sizeof(line), fileptr) != NULL) {
                        char* token = strtok(line, " ");

                        // If host is found in list.txt
                        if (strcmp(httpHost, token) == 0) {
                            foundSite = 1;
                            token = strtok(NULL, "\r\n");
                            strcpy(fileName, token);

                            printf("FOUND. File name: %s\n", fileName);
                            break;
                        }
                    }

                    fclose(fileptr);
                }

                // If URL is NOT in the cache, need to contact HTTP server
                if (!foundSite) {
                    httpServer = gethostbyname(httpHost); // Get's the server's information (including IP address) from the hostname
                    if (httpServer == NULL) {
                        printf("gethostbyname failed\n");
                    }

                    // Get the HTTP request ready to go
                    sprintf(httpSend, "GET /index.html HTTP/1.1\r\nHost: %s\r\n\r\n", httpHost);

                    /*-----Begin setup of HTTP Socket-----*/

                    // Create socket
                    if ((httpSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                        perror("socket");
                        exit(EXIT_FAILURE);
                    }

                    // Set up server information
                    bzero(&http_addr, sizeof(http_addr));
                    http_addr.sin_family = AF_INET;
                    memcpy(&http_addr.sin_addr.s_addr, httpServer->h_addr, httpServer->h_length);
                    http_addr.sin_port = htons(httpPort);

                    // Connect
                    if (connect(httpSocket, (struct sockaddr *) &http_addr, sizeof(http_addr)) < 0) {
                        perror("connect");
                        exit(EXIT_FAILURE);
                    }

                    /*-----End-----*/

                    // Send the HTTP request to the proxy server
                    if (write(httpSocket, httpSend, strlen(httpSend)) < 0) {
                        perror("write");
                        exit(EXIT_FAILURE);
                    }

                    // Get local time and save that to variables for file name
                    time(&timeNow);
                    struct tm* local = localtime(&timeNow);

                    hours   =   local->tm_hour;
                    minutes =   local->tm_min;
                    seconds =   local->tm_sec;
                    day     =   local->tm_mday;
                    month   =   local->tm_mon + 1;
                    year    =   local->tm_year + 1900;

                    // Save the time as the name of the file
                    sprintf(responseFile, "%d%02d%02d%02d%02d%02d.txt", year, month, day, hours, minutes, seconds);

                    // Open up new file for writing the response
                    fileptr = fopen(responseFile, "w");
                    if (!fileptr) {
                        perror("fopen");
                        exit(EXIT_FAILURE);
                    }

                    // Recieve the reponse from the HTTP socket
                    total = sizeof(client_message) - 1;
                    printf("Getting HTTP response...\n");

                    do {
                        bzero(client_message, sizeof(client_message));
                        usleep(10000);

                        bytes = read(httpSocket, client_message, sizeof(client_message) - 1);
                        if (bytes < 0) {
                            perror("read");
                        }

                        fprintf(fileptr, "%s", client_message);
                        if (bytes == 0 || bytes < total) {
                            close(httpSocket);
                            break;
                        }
                    } while (1);

                    fclose(fileptr);

                    char responseMsg[100];
                    sprintf(responseMsg, "...HTTP Response written to file %s\n", responseFile);
                    printf("%s", responseMsg);

                    // Open up new file for reading the response
                    fileptr = fopen(responseFile, "r");
                    if (!fileptr) {
                        perror("fopen");
                        exit(EXIT_FAILURE);
                    }

                    // Make sure the respone is 200 (OK)
                    char line[8192];

                    fgets(line, sizeof(line), fileptr);
                    char *token = strtok(line, " ");
                    token = strtok(NULL, " ");
                    fclose(fileptr);

                    // Open up new file for reading the response and to reset the pointer
                    fileptr = fopen(responseFile, "r");
                    if (!fileptr) {
                        perror("fopen");
                        exit(EXIT_FAILURE);
                    }

                    // If HTTP request response was not 200 (OK)
                    if (strcmp(token, "200") != 0) {
                        char* httpFailed = "HTTP Request has failed. Please try again.\n";
                        printf("%s\n", httpFailed);

                        while(fgets(client_message, sizeof(client_message) - 1, fileptr)) {
                            if (write(curr_fd, client_message, strlen(client_message)) < 0) {
                                perror("write");
                            }
                        }

                        fclose(fileptr);

                        if (write(curr_fd, httpFailed, strlen(httpFailed)) < 0) {
                            perror("write");
                        }

                        if (remove(responseFile) == 0) {
                            printf("\"%s\" deleted\n", responseFile);
                        } else {
                            printf("Unable to delete \"%s\"", responseFile);
                        }
                        continue;
                    }

                    // Write to which file the response was saved to
                    if (write(curr_fd, responseMsg, strlen(responseMsg)) < 0) {
                        perror("write");
                    }

                    // Write the new host in list.txt
                    if (numCached < 5) {    // If there are less than 5 hosts currently
                        fileptr = fopen("list.txt", "a");
                        if (!fileptr) {
                            perror("fopen");
                            exit(EXIT_FAILURE);
                        }

                        fprintf(fileptr, "%s %s\n", httpHost, responseFile);
                        ++numCached;
                        fclose(fileptr);
                    } else {	// If list.txt has more than 5 hosts already
                        // Skip first line (delete the HTML file with it) and then copy lines 2-4 to new file

                        fileptr = fopen("list.txt", "r");
                        if (!fileptr) {
                            perror("fopen");
                            exit(EXIT_FAILURE);
                        }

                        fileptr2 = fopen("newfile.txt", "a");
                        if (!fileptr2) {
                            perror("fopen");
                            exit(EXIT_FAILURE);
                        }

                        for (int i = 0; i < 5; ++i) {
                            fgets(line, sizeof(line), fileptr);

                            if (i == 0) {
                                char* token = strtok(line, " ");
                                token = strtok(NULL, "\r\n");

                                if (remove(token) == 0) {
                                    printf("Deleted \"%s\" successfully\n", token);
                                } else {
                                    printf("Unable to delete \"%s\"\n", token);
                                }
                            } else {
                                fprintf(fileptr2, "%s", line);
                            }
                        }

                        fprintf(fileptr2, "%s %s\n", httpHost, responseFile);

                        fclose(fileptr);
                        fclose(fileptr2);

                        if (remove("list.txt") == 0) {
                            printf("Deleted \"list.txt\" successfully\n");
                        } else {
                            printf("Unable to delete \"list.txt\"\n");
                        }

                        if (rename("newfile.txt", "list.txt") == 0) {
                            printf("Successfully renamed to \"list.txt\"\n");
                        }
                    }
                
                    // Open up file for reading the response
                    fileptr = fopen(responseFile, "r");
                    if (!fileptr) {
                        perror("fopen");
                        exit(EXIT_FAILURE);
                    }

                    // Write the cached file to the client
                    htmlStart = 0;
                    while(fgets(client_message, sizeof(client_message) - 1, fileptr)) {
                        if (!htmlStart) {
                            if((strstr(client_message, "<!")) != NULL) {
			                    htmlStart = 1;
		                    }
                        }

                        if (htmlStart) {
                            if (write(curr_fd, client_message, strlen(client_message)) < 0) {
                                perror("write");
                            }
                        } 
                    }

                    fclose(fileptr);
                } else {    // Found the cached site in list.txt
                    // Open up file for reading the response
                    fileptr = fopen(fileName, "r");
                    if (!fileptr) {
                        perror("fopen");
                        exit(EXIT_FAILURE);
                    }

                    // Write the cached file to the client
                    htmlStart = 0;
                    while(fgets(client_message, sizeof(client_message) - 1, fileptr)) {
                        if (!htmlStart) {
                            if((strstr(client_message, "<!")) != NULL) {
			                    htmlStart = 1;
		                    }
                        }

                        if (htmlStart) {
                            if (write(curr_fd, client_message, strlen(client_message)) < 0) {
                                perror("write");
                            }
                        } 
                    }

                    fclose(fileptr);
	            }
            }
        }
    }

    return 0;
}