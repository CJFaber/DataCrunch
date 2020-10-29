#include "CrunchClient.hpp"

//Client Constructor, adds the asyc connection to the io_context queue when created.
CrunchClient::CrunchClient(string ip_addr, string port_num):
	client_resolver_(client_context_), server_endpoint_(client_resolver_.resolve(ip_addr, port_num)),
	client_socket_(client_context_), socket_message_(DC_MESSAGE_SIZE), client_work_(boost::asio::make_work_guard(client_context_))
{
	StartConn();
}

CrunchClient::~CrunchClient()
{
	if(!client_context_.stopped()){
		Stop();
	}
	socket_message_.clear();
}	
	

//Spawns a new thread for the io_context to run to handle async reads and writes.
void CrunchClient::Run(void)
{
		client_thread_ = std::thread([this](){ client_context_.run(); });
}


//Closes the client socket and stops the io_context thread. Then wait on join for
//	the thread to return.
void CrunchClient::Stop(void){
	client_socket_.close();
	client_context_.stop();
	client_thread_.join();
}

//Starts the connection process of the Client to the Server
//	Callback function is currently blank.
void CrunchClient::StartConn(void)
{
	boost::asio::async_connect(client_socket_, server_endpoint_,[this](boost::system::error_code err, tcp::endpoint)
																{
																
																});
}

//Gets a data vector of size DC_MESSAGE_SIZE from the server
//	Sends a ping packet first to make ther server check if it has valid data
//	If so a valid notice is sent back followed by the actual data
//	Otherwise an invalid notice is sent followed by a vector of size one
//	The vector returned is either of size DC_MESSAGE_SIZE or one
vector<char> CrunchClient::GetData(void)
{
		//Prepare vectors
		socket_message_ = std::vector<char>(DC_MESSAGE_SIZE, 'X'); 	
		std::vector<char> ping_buf = {'0'};
		std::vector<char> send_valid = {'X'};
		
		//Write Ping packet to server
		std::future<std::size_t> send_len;
		send_len = client_socket_.async_write_some(boost::asio::buffer(ping_buf), 
													boost::asio::use_future);
		send_len.get();
		
		//Read back Ack for valid or invalid data
		//	0: invalid  |  1: valid
		std::future<std::size_t> recv_len;
		recv_len = client_socket_.async_read_some(boost::asio::buffer(send_valid, sizeof(char)),
													 boost::asio::use_future);
		recv_len.get();

		//Check if data going to be read is valid
		if(send_valid[0] == '0'){
			
			//invalid - recieve 1 Byte of data
			recv_len = client_socket_.async_receive(boost::asio::buffer(ping_buf, sizeof(char)),
														boost::asio::use_future);
		}
		else{
			//valid - Attempt to read DC_MESSAGE_SIZE bytes
			recv_len = client_socket_.async_receive(boost::asio::buffer(socket_message_, sizeof(char)*DC_MESSAGE_SIZE),
													 boost::asio::use_future);
		}
		//Wait on future get - Return either the ping vector or the socket_message_ vector
		return (recv_len.get() == 1) ? ping_buf : socket_message_;
}


