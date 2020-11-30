#include "CrunchClient.hpp"

//Client Constructor, adds the asyc connection to the io_context queue when created.
CrunchClient::CrunchClient(string ip_addr, string port_num):
	client_resolver_(client_context_), server_endpoint_(client_resolver_.resolve(ip_addr, port_num)),
	client_socket_(client_context_), socket_message_(DC_MESSAGE_SIZE,0), ping_buf_(1,'0'), serv_finished_(false), 
	waiting_on_serv_(false), sleep_time_(DC_REQ_SLEEP_TIME),
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
}


//Closes the client socket and stops the io_context thread. Then wait on join for
//	the thread to return.
void CrunchClient::Stop(void)
{
	//client_socket_.close();
	client_context_.stop();
	client_thread_.join();
}

//Starts the connection process of the Client to the Server
//	Callback function is currently blank.
void CrunchClient::StartConn(void)
{
	boost::asio::async_connect(client_socket_, server_endpoint_,[this](boost::system::error_code ec, tcp::endpoint)
																{
																	if(!ec){
																		#ifdef DEBUG
																			std::cout << "In start conn\n";
																		#endif
																		ReadFlag();
																	}																
																});
}

void CrunchClient::ReadMessage(void)
{	
	#ifdef DEBUG
		std::cout << "In ReadMessage DC_READ_MESSAGE\n";	
	#endif
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

}

std::size_t CrunchClient::MsgBytesRead(const::boost::system::error_code ec, std::size_t bytes_read)
{
	#ifdef DEBUG
		std::cout<<"In MSG BYTES READ\n";
	#endif
	return DC_MESSAGE_SIZE - bytes_read;
}

void CrunchClient::ReadFlag(void)
{
	#ifdef DEBUG
		std::cout << "In ReadFlag\n";
	#endif
	boost::asio::async_read(client_socket_, boost::asio::buffer(ping_buf_.data(), ping_buf_.size()),
							[this](boost::system::error_code ec, std::size_t length)
								{
									#ifdef DEBUG
										std::cout << "\tIn Read Flag Callback!\n";
										std::cout << "\t Flag is: " << ping_buf_[0] << std::endl;
									#endif
									if(!ec){
										if(ping_buf_[0] == '1' || ping_buf_[0] == 'w'){
											ReadMessage();
										}
										else{
											if(ping_buf_[0] == 'f'){
												serv_finished_ = true;
												waiting_on_serv_ = false;
											}
											if(ping_buf_[0] == '0'){
												waiting_on_serv_ = false;
											}
											ReadFlag();
										}
									}
									else{
										client_socket_.close();
									}
								});
}

void CrunchClient::WritePing(void)
{
	std::vector<char> ping_buf = {'0'};	
	boost::system::error_code ec;
	//std::cout << "Doing Write Ping\n";
	/*	
	boost::asio::async_write(client_socket_, boost::asio::buffer(ping_buf.data(), ping_buf.size()),
							[this](boost::system::error_code ec, std::size_t length)
							{
								if(ec){
									Stop();
								}
							});	
	*/
	boost::asio::write(client_socket_, boost::asio::buffer(ping_buf.data(), ping_buf.size()), ec);
	if(ec){
		Stop();
	}
	//else{
		//waiting_lock_.lock();
		//waiting_on_serv_ = true;
		//waiting_lock_.unlock();
	//}
}										


void CrunchClient::ReadHandle(const::boost::system::error_code ec)
{
	if(!ec){
		//Lock message topic
		#ifdef DEBUG
			std::cout << "In read Handle\n";
		#endif
		message_lock_.lock();
		//Write read message to Queue
		message_queue_.push(socket_message_);
		//Clear Message
		std::fill(socket_message_.begin(), socket_message_.end(), 0);
		//Unlock Message topic
		message_lock_.unlock();
		//Go back to do ReadPing to start transaction
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
//	The vector returned is either of size DC_MESSAGE_SIZE or one
vector<char> CrunchClient::GetData(void)
{
	//Create return vector
	std::vector<char> ret_vec;
	//Signal we want data
	//while(ret_vec.empty()){	
		message_lock_.lock();
		//If our message queue is not empty and not waiting on a request from the server
		if(message_queue_.size() > 0) // && !waiting_on_serv_)
		{
			#ifdef DEBUG
				std::cout << "Message_queue_.size() is not zero!\n";
			#endif 
			ret_vec = message_queue_.front();
			message_queue_.pop();
			message_lock_.unlock();
			return ret_vec;
		}
		else
		{
			//Nothing in message queue, return f or wait on message
			message_lock_.unlock();
			if(serv_finished_){	
				#ifdef DEBUG
					std::cout<<"Serv_finished flag set\n";		
				#endif
				ret_vec = std::vector<char>(1, 'f');
			}
			else{	
				#ifdef DEBUG
					//std::cout<<"Sending empty vector back\n";
				#endif
				//We didn't have anything in the message queue and didn't get an f
				// do write ping to let the server know we want data, if we haven't alreday sent a request.
				if(!waiting_on_serv_){
					//Signal we want data
					#ifdef DEBUG
                    	std::cout << "signaling we want data\n";
                	#endif
					WritePing();
					waiting_on_serv_ = true;
				}
				//else{
					//We have already sent a request and are still waitng on data from the server
					//Go to sleep for 200ms try again

				//	#ifdef DEBUG
						//std::cout << "Going to sleep\n";
				//	#endif
					//usleep(sleep_time_);
			}
			
		}
		return ret_vec;
	}

	//}
	//#ifdef DEBUG
	//	std::cout << "Got a vector to return!\n";
	//#endif
	//return ret_vec;
//}



