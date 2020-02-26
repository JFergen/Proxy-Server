To compile:
	gcc server.c -o server
	gcc client.c -o client

To run: (server must be on cse01.cse.unt.edu virtual machine, client must be on cse02.cse.unt.edu)
    ./server <port>	(must be started first)
    ./client <port>	(only one client at a time)

Usage:
	A single client connects to the proxy server and can input urls for HTML requests. The proxy server will first check "list.txt" to see if that site has been cached already. If it is, the server sends the cached page to the client. If the site has not been cached in "list.txt", the server makes a request to the input host and saves the HTML page and sends it to the client, while caching it in list.txt.
