/**********************************************************************

   File          : CommunicationServices.h

   Description   : Network communication needs. A Singleton but can 
    				actually just be static

   Last Modified : 
   By            : Wilmarc Lopez

***********************************************************************/

#ifndef COMM_SERVICES_H
#define COMM_SERVICES_H

#include "MessageProcessor.h"
#include "Message.h"
#include "Logger.h"

#define BACKLOG 15	//listening buffer size

class CommunicationServices{
		public:
			static CommunicationServices* getInstance();  
			int initServer(MessageProcessor *mp_p, int tpProtocol, 
					char* port);	//init for server
			int start();	//returns -1 when not started properly

		private:
			CommunicationServices(){}  // Private so that it can  not be called
			CommunicationServices(CommunicationServices const&){}             // copy constructor is private
			CommunicationServices& operator=(CommunicationServices const&){}  // assignment operator is private

			static CommunicationServices* INSTANCE;
			static Logger *logger;
			
			MessageProcessor *mp;
			int mainSocket;			
			
			/**********************************************************************
			Below are revised versions from: 
				http://www.cse.psu.edu/~tjaeger/cse473-s08/cse473-s08-p2.html
			***********************************************************************/
			/**********************************************************************

				Function    : receiveReq
				Description : receive a request  over the wire
				Inputs      : sock - the socket to receive the file over
							  msg - reference to Message output
				Outputs     : 0 if successful, -1 if failure

			***********************************************************************/

			int receiveReq( int sock, Message *msg );			 
			
			
	   
		public:		
			
			/**********************************************************************

				Function    : sendMessage
				Description : send data over the socket
				Inputs      : sock - server socket
							  hdr - the header structure
							  block - the block to send
				Outputs     : bytes read if successful, -1 if failure

			***********************************************************************/

			int sendMessage( int sock, MessageHeader *hdr, char *block );
			
			/**********************************************************************

				Function    : connectClient
				Description : connnect a client to the server
				Inputs      : addr - the address ("a.b.c.d/port")
				* 				port - port of server to connect to
				Outputs     : file handle if successful, -1 if failure

			***********************************************************************/
			static int connectClient( char* addr, char* port );

			/**********************************************************************

				Function    : serverConnect
				Description : connnect a server socket to listen for
				Inputs      : port - port to bind
								tpProtocol - transport layer protocol
				Outputs     : file handle if successful, -1 if failure

			***********************************************************************/
			static int serverConnect( char* port, int tpProtocol );

			/**********************************************************************

				Function    : serverAccept
				Description : get a new connection on the server socket
				Inputs      : sock - server socket
				Outputs     : file handle if successful, -1 if failure

			***********************************************************************/
			static int serverAccept( int sock );

			/**********************************************************************

				Function    : recvData
				Description : receive data from the socket
				Inputs      : sock - server socket
							  blk - block to put data in
							  sz - maxmimum size of buffer
							  minsz - minimum bytes to read
				Outputs     : bytes read if successful, -1 if failure

			***********************************************************************/
			static int recvData( int sock, char *blk, int sz, int minsz );

			/**********************************************************************

				Function    : sendData
				Description : send  data to the socket
				Inputs      : sock - server socket
							  blk - block to put data in
							  len - length of data to send
				Outputs     : 0 if successful, -1 if failure

			***********************************************************************/
			static int sendData( int sock, char *blk, int len );
			
		protected:
			// get sockaddr, IPv4 or IPv6:
			static void *get_in_addr(struct sockaddr *sa);			
};

#endif
