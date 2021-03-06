#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>


#include "CommunicationServices.h"
#include "Logger.h"

CommunicationServices* CommunicationServices::INSTANCE = NULL;
Logger* CommunicationServices::logger = Logger::getInstance();

CommunicationServices* CommunicationServices::getInstance()
{
	if (!INSTANCE)
		INSTANCE = new CommunicationServices;
	
	return INSTANCE;
}

int CommunicationServices::initServer(MessageProcessor *mp_p, 
	int tpProtocol, char* port)
{
	mp = mp_p;	//mp should have its tpm set here
	
	mainSocket = serverConnect(port, tpProtocol);
	
	return 0; //temporary
}



int CommunicationServices::start()
{
	int errored = 0, newsock = 0;
    fd_set readfds;
    
	/* check if initialized */
	if ( !mp || !mainSocket )
	{
		logger->error("CommunicationServices not initialized.\n");
		return -1;
	}
	
	/* Repeat until the socket is closed */
    while ( !errored )
    {
         FD_ZERO( &readfds );
         FD_SET( mainSocket, &readfds );
         if ( select(mainSocket+1, &readfds, NULL, NULL, NULL) < 1 )
         {
             /* Complain, explain, and exit */
              char msg[128];
              sprintf( msg, "failure selecting server connection [%.64s]\n",
                       strerror(errno) );
              logger->error( msg );
              errored = 1;
         }
         else
         {
			/* Accept the connect, receive request, and return */
			if ( (newsock = serverAccept(mainSocket)) != -1 )
			{
				Message *msg = new Message;	//mp should free this

				// fill msg with received data from the socket
				if (receiveReq( newsock, msg ) < 0) {
					errored = 1;					
					deallocMsg(msg);	//deallocate when error
				}
				
				if ( !errored ) {
					//pass message to MessageProcessor
					errored = mp->process(msg);
				}			
				
				fflush( stdout );
			}
			else {
				/* Complain, explain, and exit */
				char msg[128];
				sprintf( msg, "failure accepting connection [%.64s]\n", 
					  strerror(errno) );
				logger->error( msg );
				errored = 1;
			}
         }
    }
    
    return 0;
}


int CommunicationServices::serverConnect( char* port, int tpProtocol )
{
	int sockfd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    int yes=1;    
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = tpProtocol;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        char msg[128];
		sprintf(msg, "getaddrinfo: %s\n", gai_strerror(rv));
        logger->error(msg);
        
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            logger->error("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            logger->error("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            logger->error("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, BACKLOG) == -1) {
        logger->error("listen");
        exit(1);
    }   

	char s[INET6_ADDRSTRLEN];
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
            
	char msg[128];
	sprintf(msg, "Server listening at %s:%s ...", s,port);
	logger->info(msg);
        
	return sockfd;	
}

int CommunicationServices::serverAccept( int sock )
{
	struct sockaddr_in inet;
	unsigned int inet_len = sizeof(inet), nsock;

     // Do the accept
     if ( (nsock = accept(sock, (struct sockaddr *)&inet, &inet_len)) == 0 )
     {
        /* Complain, explain, and return */
        char msg[128];
        sprintf( msg, "failed server socket accept [%.64s]\n", 
                       strerror(errno) );
        logger->error( msg );
        exit( -1 );
     }

     /* Return the new socket */
     return( nsock );
}


int CommunicationServices::receiveReq( int sock, Message *msg )
{		
	//point to header
	MessageHeader *hdr = &msg->hdr;

	// read the message header 
	recvData( sock, (char *)hdr, sizeof(MessageHeader), 
		   sizeof(MessageHeader) );
	hdr->length = ntohs(hdr->length);
	assert( hdr->length < MAX_BODY_SIZE );
	hdr->msgtype = ntohs( hdr->msgtype );
	
	// Display header	
	cout<< "Received message on socket" << sock << " with msgtype = "
		<< hdr->msgtype << "and length =" << hdr->length<<"\n";
				   

	// set the socket
	msg->sock = sock;

	// set body size
	msg->body = new char[hdr->length];	

	// read the message body
	if ( hdr->length > 0 )
		return(  recvData( sock, msg->body, hdr->length, hdr->length ) );

	return( 0 );
}

/**********************************************************************

    Function    : recvData
    Description : receive data from the socket
    Inputs      : sock - server socket
                  blk - block to put data in
                  sz - maxmimum size of buffer
                  minsz - minimum bytes to read
    Outputs     : bytes read if successful, -1 if failure

***********************************************************************/

int CommunicationServices::recvData( int sock, char *blk, int sz, int minsz )
{
     /* Keep reading until you have enough bytes */
     int rb = 0, ret;
     do 
     {
          /* Receive data from the socket */
          if ( (ret=recv(sock, &blk[rb], sz-rb, 0)) == -1 )
          {
               /* Complain, explain, and return */
               char msg[128];
               sprintf( msg, "failed read error [%.64s]\n", 
                        strerror(errno) );
               logger->error( msg );
               exit( -1 );
          }

          /* Increment read bytes */
          rb += ret;
     }
     while ( rb < minsz );

     /* Return the new socket */
     logger->printBuffer( "recv data : ", blk, sz ); 
     return( 0 );
}

/**********************************************************************

    Function    : sendMessage
    Description : send data over the socket
    Inputs      : sock - server socket
                  hdr - the header structure
                  block - the block to send
    Outputs     : bytes read if successful, -1 if failure

***********************************************************************/

int CommunicationServices::sendMessage( int sock, MessageHeader *hdr, char *block )
{
     int real_len = 0;

     /* Convert to the network format */
     real_len = hdr->length;
     hdr->msgtype = htons( hdr->msgtype );
     hdr->length = htons( hdr->length );
     if ( block == NULL )
          return( sendData( sock, (char *)hdr, sizeof(MessageHeader) ) );
     else 
          return( sendData(sock, (char *)hdr, sizeof(MessageHeader)) ||
                  sendData(sock, block, real_len) );
}

/**********************************************************************

    Function    : sendData
    Description : send  data to the socket
    Inputs      : sock - server socket
                  blk - block to send
                  len - length of data to send
    Outputs     : 0 if successful, -1 if failure

***********************************************************************/

int CommunicationServices::sendData( int sock, char *blk, int len )
{

     /* Send data using the socket */
     if ( send(sock, blk, len, 0) != len )
     {
        /* Complain, explain, and return */
        logger->error( "failed socket send [short send]" );
		printf( " len = %d\n", len );
        exit( -1 );
     }

     logger->printBuffer( "sent data : ", blk, len ); 

     return( 0 );
}
/**********************************************************************

    Function    : connectClient
    Description : connnect a client to the server
    Inputs      : address - the address ("a.b.c.d")
					port - port of server
    Outputs     : file handle if successful, -1 if failure

***********************************************************************/

int CommunicationServices::connectClient( char* address, char* port )
{	
	struct addrinfo hints, *servinfo, *p;	
	int sockfd, rv;	
	char s[INET6_ADDRSTRLEN];
	
	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
		char msg[128];
		sprintf(msg, "getaddrinfo: %s\n", gai_strerror(rv));
        logger->error(msg);
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            logger->error("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            logger->error("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        logger->error("client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
            
	char msg[128];
	sprintf(msg, "Client: Connected to %s:%s\n", s, port);
    logger->info(msg);

    freeaddrinfo(servinfo); // all done with this structure
	
	return( sockfd );
}

// get sockaddr, IPv4 or IPv6:
void *CommunicationServices::get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
