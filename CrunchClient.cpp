#include "CrunchClient.hpp"
#ifdef DEBUG
	#include <thread>
#endif

//Client Constructor, adds the asyc connection to the io_context queue when created.
CrunchClient::CrunchClient(string ip_addr, string port_num):
	client_resolver_(client_context_), 
	server_endpoint_(client_resolver_.resolve(ip_addr, port_num)),
	client_socket_(client_context_), 
	socket_message_(DC_MESSAGE_SIZE,0), 
	ping_buf_(1,'0'), 
	serv_finished_(false), 	
	client_work_(boost::asio::make_work_guard(client_context_))
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
		WritePing();
}


//Closes the client socket and stops the io_context thread. Then wait on join for
//	the thread to return.
void CrunchClient::Stop(void)
{
	//client_socket_.close();
	client_context_.stop();
	client_thread_.join();
}

//Starts the connection process of the Client to the Server, calls ReadFlag in lambda
void CrunchClient::StartConn(void)
{
	#ifdef DEBUG
		std::cout << "In StartConn Call\n";
	#endif
	boost::asio::async_connect(client_socket_, server_endpoint_,[this](boost::system::error_code ec, tcp::endpoint)
																{
																	if(!ec){
																		#ifdef DEBUG
																			std::cout << "In start conn lambda\n";
																			std::cout << "ThreadID is: " << std::this_thread::get_id() << std::endl;
																		#endif
																		ReadFlag();
																	}																
																});

	#ifdef DEBUG
		std::cout << "Leaving StartConn Call\n";
	#endif
}

void CrunchClient::ReadMessage(void)
{	
	#ifdef DEBUG
		std::cout << "In ReadMessage DC_READ_MESSAGE, socket_message_ size is:" 
				  << socket_message_.size() << std::endl;
			
	#endif
	/*
	boost::asio::async_read(client_socket_, boost::asio::buffer(socket_message_, socket_message_.size()),
							boost::bind(&CrunchClient::MsgBytesRead, this,
										boost::asio::placeholders::error,
										boost::asio::placeholders::bytes_transferred
							),
							//Read handle after total message size has been reached
							boost::bind(&CrunchClient::ReadHandle, this,
										boost::asio::placeholders::error
							)
				);
	*/
	boost::asio::async_read(client_socket_, boost::asio::buffer(socket_message_, socket_message_.size()),
							//Read handle after total message size has been reached
							boost::bind(&CrunchClient::ReadHandle, this,
										boost::asio::placeholders::error
							)
				);

}

//Returns the number of bytes read, not used at the moment
std::size_t CrunchClient::MsgBytesRead(const::boost::system::error_code ec, std::size_t bytes_read)
{
	#ifdef DEBUG
		std::cout<<"In MSG BYTES READ\n";
	#endif
	return DC_MESSAGE_SIZE - bytes_read;
}

//Read and interpret a flag from the server, calls ReadMessage
void CrunchClient::ReadFlag(void)
{
	//Alright in read flag set it so that we can't read a 
	#ifdef DEBUG
		std::cout << "In ReadFlag\n";
		std::cout << "ThreadID: " << std::this_thread::get_id() << std::endl;
	#endif
	boost::asio::async_read(client_socket_, boost::asio::buffer(ping_buf_.data(), ping_buf_.size()),
							[this](boost::system::error_code ec, std::size_t length)
								//Begin Lambda fucntion (ec, length)
								{
									#ifdef DEBUG
										std::cout << "\tIn Read Flag Callback!\n";
										std::cout << "\t Flag is: " << ping_buf_[0] << std::endl;
										std::cout << "\tThread ID for callback: "<<std::this_thread::get_id() <<std::endl;
									#endif
									if(!ec){
										switch(ping_buf_[0])
										{
											case 't':
											{
												#ifdef DEBUG
												std::cout << "Got a t back from the server\n";
												#endif
												//Jump to read message loop
												ReadMessage();
												break;
											}
											case 'w':
											{
												#ifdef DEBUG
												std::cout << "Got a w back from the server\n";
												#endif
												//No Data avaliable for send yet jump to read message but lock the 
												// channel so the main thread can't try and continuely send ping messages
												//Jump to Read message
												ReadMessage();
												break;
											}
											case 'f':
											{
												//Server is finished
												#ifdef DEBUG
												std::cout << "Got f from async read\n";
												#endif
												//Just incase the client for some reason is stuck in the get data function
												// and the server has lagged in sending the f flag
												std::unique_lock<std::mutex> msg_lck(message_lock_);
												serv_finished_ = true;
												message_sig_.notify_one();
												msg_lck.unlock();
												//Jump back to Read flag
												ReadFlag();
												break;
											}
											default:
											{
												#ifdef DEBUG
												//Server Sent something weird
												std::cout << "Got into default case\n";
												#endif
												std::cout<<"Client read something weird\n";
												client_socket_.close();
											}
										}
									}
									else{
										client_socket_.close();
									}
									std::cout << "Leaving ReadFlag Lambda\n";
								});
}

//Writes a ping to the server to request data
void CrunchClient::WritePing(void)
{
	std::vector<char> ping_buf = {'0'};	
	boost::system::error_code ec;
	boost::asio::write(client_socket_, boost::asio::buffer(ping_buf.data(), ping_buf.size()), ec);
	if(ec){
		Stop();
	}
}										

//Reads a message from the server, calls Read Flag when returning
void CrunchClient::ReadHandle(const::boost::system::error_code ec)
{
	if(!ec){
		//Lock message topic
		#ifdef DEBUG
			std::cout << "In read Handle\n";
		#endif
		std::unique_lock<std::mutex> msg_lck(message_lock_);
		//Write read message to Queue
		message_queue_.push(socket_message_);
		//Signal wakeup
		message_sig_.notify_one();
		//Unlock the message lock
		msg_lck.unlock();
		//Clear Message
		std::fill(socket_message_.begin(), socket_message_.end(), 0);
		ReadFlag();			
	}
	else{
		Stop();
	}
}



//Gets a data vector of size DC_MESSAGE_SIZE from the server
//	Sends a ping packet first to make ther server check if it has valid data
//	If so a valid notice is sent back followed by the actual data
//	Otherwise an invalid notice is sent followed by a vector of size one
//	The vector returned is either of size DC_MESSAGE_SIZE or one (Padding is handled on server side)
vector<char> CrunchClient::GetData(void)
{
	//Create return vector
	std::vector<char> ret_vec(0,'0');
	//Signal we want data
		//If our message queue is not empty and not waiting on a request from the server

		if(message_queue_.empty() && serv_finished_){
			#ifdef DEBUG
				std::cout << "Consumer: message_queue and server finished flag sent";
			#endif 
			return std::vector<char>(1, 'f');
		}
		std::unique_lock<std::mutex> lock(message_lock_);
		while(message_queue_.empty()) // && !waiting_on_serv_)
		{
			if(serv_finished_) return std::vector<char>(1, 'f');
			#ifdef DEBUG
				std::cout << "Consumer: Message Queue empty going to sleep";
			#endif 
			message_sig_.wait(lock);
		}
		#ifdef DEBUG
			std::cout << "Consumer: Got past lock\n";
		#endif
		ret_vec = message_queue_.front();
		message_queue_.pop();	
		//Return empty vector, Waiting on Data from server
		return ret_vec;
	}




