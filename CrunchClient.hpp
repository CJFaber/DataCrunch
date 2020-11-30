#ifndef CRUNCH_CLIENT_HPP_
#define CRUNCH_CLIENT_HPP_ 

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <unistd.h>
#include <memory>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include "CrunchDefines.h"

using boost::asio::ip::tcp;
using std::vector;
using std::queue;
using std::string;

// Client for the Data Crunch Server (CrunchServer)
//	Spawns a thread for boost::asio::io_context and keeps the thread
//	and socket alive as long as the class is alive.
class CrunchClient
{
	public:
		CrunchClient(std::string ip_addr, std::string port_num);		//Constructor
		~CrunchClient();												//Destructor

		//Returns a vector of variable size from server depending on valididy ( size == DC_MESSAGE_SIZE || 1 )
		vector<char> GetData(void);

		//Callback function for read returns the number of bytes read 
		std::size_t MsgBytesRead(const::boost::system::error_code ec, std::size_t bytes_read);
		
		//Callback function for handling read once desired number of bytes have been read
		void ReadHandle(const::boost::system::error_code ec);

		//Read a flag from the server
		void ReadFlag(void);
		//Read a message from the server
		void ReadMessage(void);
		//Writes a Ping to the server to request data
		void WritePing(void); 

		//Thread and socket control 
		void Run(void);		//Starts the io_context needs to be called before GetData()
		void Stop(void);	//Stops the io_context thread and closes the socket
		
	private:
		//Starts the socket connection (tcp)
		void StartConn(void);
	
		//Boost ASIO types for networking 				//ORDERING IMPORTANCE
		boost::asio::io_context		 client_context_;	//
		tcp::resolver				 client_resolver_;	//
		tcp::resolver::results_type	 server_endpoint_;	//
		tcp::socket					 client_socket_;	//

		//Vector to hold socket message read from server
		bool						 serv_finished_;
		bool						 waiting_on_serv_;
		unsigned int				 sleep_time_;		//Time in USec to sleep before trying to send another ping message
		vector<char>				 socket_message_;	//Message from server (always size DC_MESSAGE_SIZE)		
		vector<char>				 ping_buf_;			//Valid data flag
		boost::mutex				 message_lock_;		//Queue Lock for pop/push messages from queue
		queue<vector<char>>			 message_queue_;	//Message Queue of data recieved from server.
				

		//Thread and work handle for io_context thread
		std::thread					 client_thread_;	
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type>  client_work_;
};

#endif /*CRUNCH_CLIENT_HPP_*/
