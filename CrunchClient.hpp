#ifndef CRUNCH_CLIENT_HPP_
#define CRUNCH_CLIENT_HPP_ 

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <unistd.h>
#include <memory>
#include <ctime>
#include <ratio>
#include <chrono>


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
		//	Will return a vector of size one with the value 'f' if the server has finished sending data
		//  Blocking call will put thread to sleep while waiting for data.
		vector<char> GetData(void);

		//Callback function for read returns the number of bytes read, not used at the moment 
		std::size_t MsgBytesRead(const::boost::system::error_code ec, std::size_t bytes_read);
		
		//Callback function for handling read once desired number of bytes have been read
		void ReadHandle(const::boost::system::error_code ec);

		//Read and interpet a flag from the server
		void ReadFlag(void);
		//Read a message from the server
		void ReadMessage(void);
		//Writes a Ping to the server to request data
		void WritePing(void); 

		//Thread and socket control 
		void Run(void);		//Starts the io_context needs to be called before GetData()
		void Stop(void);	//Stops the io_context thread and closes the socket
		
	private:
		//Starts the socket connection (tcp), calls ReadFlag in lambda
		void StartConn(void);
	
		//Boost ASIO types for networking 				//ORDERING IMPORTANCE
		boost::asio::io_context		 client_context_;	//
		tcp::resolver				 client_resolver_;	//
		tcp::resolver::results_type	 server_endpoint_;	//
		tcp::socket					 client_socket_;	//

		//Vector to hold socket message read from server
		bool						 serv_finished_;	//Flag for holding the done state of the server
		vector<char>				 socket_message_;	//Live data from server (always size DC_MESSAGE_SIZE)		
		vector<char>				 ping_buf_;			//Valid data flag
		
		std::mutex					 message_lock_;		//Queue Lock for pop/push messages from queue
		std::condition_variable		 message_sig_;		//Signal for message queue
		queue<vector<char>>			 message_queue_;	//Message Queue of data recieved from server.
				

		//Thread and work handle for io_context thread
		std::thread					 client_thread_;	
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type>  client_work_;
};


class TimeStamp
{
	public:
	//Create/Destroy
	TimeStamp();
	//~TimeStamp();

	//Takes the current time stamp and adds it to the list
	void Clock(void);

	//Dumps out all the values of the list
	void Dump(void);

	//Find the time between two iteration points, given iteration a < iteration b
	// Values are 0th indexed	
	void DumpSpan(int a, int b);

	void CheckIn(void); 
	
	private:
	std::chrono::high_resolution_clock::time_point 		current_step_;
	std::chrono::high_resolution_clock::time_point 		prev_step_;
	std::vector<std::chrono::microseconds>        		iter_time_stamps_;
	std::chrono::microseconds 							hold_duration_;
		
};
#endif /*CRUNCH_CLIENT_HPP_*/
