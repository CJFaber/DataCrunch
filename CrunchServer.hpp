#ifndef CRUNCH_SERVER_HPP_
#define CRUNCH_SERVER_HPP_ 

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <ctime>
#include <ratio>
#include <chrono>

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
		ServerConnection(tcp::socket server_socket, queue<vector<char>>& server_queue, std::mutex& mutex, 
							std::condition_variable& cond_var, bool& flag1);
	private:
		
		//Reads ping from client
		void ping_read();
		
		//Writes live_data_vector to client
		void DataWrite();

		//Writes a flag to the client given a flag vector
		void FlagWrite(vector<char>);

		//Handle for the async read ping - Starts the message process
		void startup_on_ping(boost::system::error_code ec, std::size_t msg_len);
		
		//Main loop - Perpares data for async write, uses mutex and condition variable on message queue
		void ServeData();
	
		//Writes a valid/finished flag to the client
		bool FlagPost();

		//Buffer for ping packet
		boost::asio::streambuf		streambuf;

		//Delimiter for startup_on_ping
		char						Delim_;	
	
		//boost::asio socket (tcp)
		tcp::socket 				conn_socket_;
		
		//Mutex for message 		
		std::mutex&					server_msg_lock_;
		
		//Condition variable for the status of the message queue (false for empty true for has data)
		std::condition_variable&	server_msg_sig_;

		//Data to be sent to client, removed from the server message queue
		vector<char>				live_data_;
		
		//Queue of messages waiting to be sent from the server.
		queue<vector<char>>&		server_msg_queue_;
		
		//Ref to the finishedflag in the server class
		bool&						server_finished_flag_;
		bool						queue_empty_flag_;	
};	

//DataCrunch Server class, listens on port and starts new sockets when requested from all IP addrs 
class CrunchServer
{
	public:
		//Loads a message into the message queue, waiting to be sent
		void LoadData(vector<char> Msg);

		//Called when there are no more messages to send
		void PostEndMessage(void);

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
		
		//Mutex for message_queue, lock when accessing queue
		std::mutex					message_lock_;

		//Condition variable for signaling when data is in the 
		std::condition_variable 	message_sig_;

		//Holds messages to be sent to clients		
		queue<vector<char>>			message_queue_;

		//Flag denoting we are finished
		bool						finished_flag_;
		
		//Thread and work guard for io_context
		std::thread					server_thread_;	
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type>  server_work_;
		
		void StartAccept(void);
		
		
};

void TimeStamp(void);

#endif /*CRUNCH_SERVER_HPP_*/
