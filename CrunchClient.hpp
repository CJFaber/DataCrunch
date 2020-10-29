#ifndef CRUNCH_CLIENT_HPP_
#define CRUNCH_CLIENT_HPP_ 

#include <iostream>
#include <vector>
#include <future>
#include <thread>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/use_future.hpp>

#include "CrunchDefines.h"

using boost::asio::ip::tcp;
using std::vector;
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
		vector<char>				 socket_message_;
		
		//Thread and work handle for io_context thread
		std::thread					 client_thread_;	
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type>  client_work_;
};

#endif /*CRUNCH_CLIENT_HPP_*/
