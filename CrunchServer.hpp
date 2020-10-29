#ifndef CRUNCH_SERVER_HPP_
#define CRUNCH_SERVER_HPP_ 

#include <iostream>
#include <vector>
#include <queue>
#include <future>

#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include "CrunchDefines.h"

using boost::asio::ip::tcp;
using std::vector;
using std::string;
using std::queue;

//ServerConnection - Connection handle for CrunchServer, generated for every socket connection
class ServerConnection : public std::enable_shared_from_this<ServerConnection>
{
	public:	
		//Send Data
		void SendData(vector<char> msg);

		//Return socket handle
		tcp::socket& GetSocket();

		//Start the process of listening for a ping packet
		void Start();
	
		//Server Constructor
		ServerConnection(tcp::socket server_socket, queue<vector<char>>& server_queue, boost::mutex& mutex);
	private:
		void async_read();
		void async_write();
		
		//Handle for async_read function
		void on_read(boost::system::error_code ec, std::size_t msg_len);
		//Handle for async_write function
		void on_write(boost::system::error_code ec, std::size_t msg_size);
		
		//Perpares data for async write
		void DataPost();
	
		//Safely removes data from the shared message queue
		vector<char> SafePop();

		//Buffer for ping packet
		boost::asio::streambuf	streambuf;
		//Delimiter for 
		char					Delim_;	
	
		//boost::asio socket (tcp)
		tcp::socket 			conn_socket_;
		
		//Mutex for message queue
		boost::mutex&			server_lock_;

		//Data to be sent to client, removed from message queue or vector containing '0'
		vector<char>			live_data_;
		
		//Queue of messages to be written (valid message, data message)
		queue<vector<char>>		write_queue_;

		//Queue of messages waiting to be sent from the server.
		queue<vector<char>>&	server_msg_queue_;
};	

//DataCrunch Server class, listens on port and starts new sockets when requested from all IP addrs 
class CrunchServer
{
	public:
		//Loads a message into the message queue, waiting to be sent
		void LoadData(vector<char> Msg);

		//Starts the io_context in a thread for the server
		void Run(void);

		//Stops the io_context thread and closes the server
		void Stop(void);
		
		//Constructor
		CrunchServer(void);

	private:
		//Boost asio members
		boost::asio::io_context		server_context_;
		tcp::acceptor				server_acceptor_;
		
		//Mutex for message_queue
		boost::mutex				message_lock_;

		//Holds messages to be sent to clients		
		queue<vector<char>>			message_queue_;

		//Thread and work guard for io_context
		std::thread					server_thread_;	
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type>  server_work_;
		
		void StartAccept(void);
		
		
};

#endif /*CRUNCH_SERVER_HPP_*/
