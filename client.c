/* 
    Joseph Fergen
    Proxy Server Client
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <regex.h>
#include <sys/select.h>

#define URL_REGEX "^www\.?[a-z0-9]+([\-\.]{1}[a-z0-9]+)*\.[a-z]{2,5}(:[0-9]{1,5})?(\/.*)?$"
 
int main (int argc, char *argv[]) {
    // If not given the correct information
    if (argc != 2) {
        fprintf(stderr, "usage: %s <svr_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd, reti, activity, bytes, total;
    struct sockaddr_in serv_addr;
    regex_t regex;
    fd_set readfds;

    // AF_INET - IPv4 IP , Type of socket, protocol
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up client information with server's
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));          // Server port number
    if (inet_pton(AF_INET,"129.120.151.94",&(serv_addr.sin_addr)) < 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // Initalize regex
    if ((reti = regcomp(&regex, URL_REGEX, REG_EXTENDED | REG_NEWLINE)) != 0) {
        perror("regcomp");
        exit(EXIT_FAILURE);
    }
 
    // Continue until disconnect or quit
    while (1) {
        char recvline[4186];
        char url[200];

        printf("url: ");
        fflush(stdout);

        // Clear the socket set
        FD_ZERO(&readfds);

        // Add socket and stdin to set
        FD_SET(sockfd, &readfds);   // Socket   (server)
        FD_SET(0, &readfds);        // Stdin    (User-input)

        if ((activity = select(sockfd +1, &readfds, (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0)) < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // Message is coming from the server
        if (FD_ISSET(sockfd, &readfds)) {
            bzero(recvline, sizeof(recvline));  // Clear out server message
            printf("\nFROM SERVER: ");

            // Get message from the server
            total = sizeof(recvline) - 1;
            do {
                bzero(recvline, sizeof(recvline));
                usleep(100);

                bytes = read(sockfd, recvline, sizeof(recvline) - 1);
                if (bytes < 0) {
                    perror("read");
                }

                printf("%s\n", recvline);
                if (bytes == 0 || bytes < total) {
                    break;
                }
            } while (1);

            // If server has stopped
            if (recvline[0] == '\0') {
                printf("Server disconnected\n");
                printf("Closing...\n");
                break;
            } 
        }

        // Message is coming from stdin
        if (FD_ISSET(0, &readfds)) {
            fgets(url, sizeof(url), stdin);             // Gets the input from the user

            reti = regexec(&regex, url, 0, NULL, 0);    // Check regular expression to ensure it is a valid URL

            // If user does not enter a valid URL, continue until it is valid
            while (reti == REG_NOMATCH) {
                if (strcmp(url, "quit\n") == 0) {
                    if (write(sockfd, url, strlen(url)) < 0) {
                        perror("write");
                        exit(EXIT_FAILURE);
                    }
                
                    break;
                }

                printf("Invalid URL. Please enter a URL starting with \"www.\"\n");
                printf("url: ");

                fgets(url, sizeof(url), stdin);             // Gets the input from the user
                reti = regexec(&regex, url, 0, NULL, 0);    // Check regular expression to ensure it is a valid URL
            }

            // Send the message to the server
            if (write(sockfd, url, strlen(url)) < 0) {
                perror("write");
                exit(EXIT_FAILURE);
            }
        }       
    }

    regfree(&regex);
    close(sockfd);
    return 0;
}