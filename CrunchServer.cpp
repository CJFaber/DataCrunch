#include "CrunchServer.hpp"

CrunchServer::CrunchServer(void):
	server_acceptor_(server_context_, tcp::endpoint(tcp::v4(), DC_PORT)),
	server_work_(boost::asio::make_work_guard(server_context_))
{
	finished_flag_ = false;
	data_in_queue_ = false;
	server_acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
	server_acceptor_.listen();
	StartAccept();
}

void CrunchServer::LoadData(vector<char> Msg)
{
 	message_lock_.lock();	
	message_queue_.push(Msg);
	data_in_queue_ = true;
	message_lock_.unlock();
}

void CrunchServer::PostEndMessage(void)
{
	message_lock_.lock();
	finished_flag_ = true;
	message_lock_.unlock();
}


void CrunchServer::Run(void)
{
	server_thread_ = std::thread([this](){server_context_.run();});
}

void CrunchServer::Stop(void)
{
	server_context_.stop();
	server_thread_.join();

}


void CrunchServer::StartAccept(void)
{
	server_acceptor_.async_accept(
		[this](boost::system::error_code ec, tcp::socket server_socket)
		{
			if(!ec){
				std::make_shared<ServerConnection>(std::move(server_socket), message_queue_, message_lock_, finished_flag_, data_in_queue_)->Start();
			}
			StartAccept();
		});
}

ServerConnection::ServerConnection(tcp::socket server_socket, queue<vector<char>>& server_queue, boost::mutex& mutex, bool& flag1, bool& flag2) : 
	conn_socket_(std::move(server_socket)), server_msg_queue_(server_queue), server_lock_(mutex), Delim_('0'), 
	server_finished_flag_(flag1), data_in_server_queue_(flag2)
{
	//std::cout << "Checking write queue:\n\t size is: "<<write_queue_.size();
	//Empty Constructor

}

void ServerConnection::Start()
{
	async_read();
}


//Read until we get a '0' from the client which will trigger us to send one ping packet and everything in write queue datagram
void ServerConnection::async_read()
{
	boost::asio::async_read_until(conn_socket_, streambuf, "0", 
									[self = shared_from_this()] (boost::system::error_code ec, std::size_t msg_len)
									{
										self->on_read(ec, msg_len);
									});
}

void ServerConnection::DataWrite()
{	
	#ifdef DEBUG
		std::cout<<"In Data Write Going to write Data to client " << std::endl;
	#endif
	boost::system::error_code ec;
	boost::asio::write(conn_socket_, boost::asio::buffer(write_queue_.front().data(),
																	(sizeof(char) * write_queue_.front().size())));
	if(!ec){
        write_queue_.pop();
        if(!write_queue_.empty()){
            //std::cout << "Data left in queue send sync Packet|\n";
            SyncPacketWrite();
        }
        else{
            //std::cout <<"data is gone\n";
            NotifyEnd();
            //Write queue is empty go back and wait for input from client.
    	}
	}
    else
    {
    	conn_socket_.close(ec);
	}
				
}
//Function for writing a sequence packet between each DataAsyncWrite while there is data in the write queue
void ServerConnection::SyncPacketWrite()
{
	//std::cout << "sending sync packet write value: " << '1' <<std::endl;
	std::vector<char> valid(1, '1');
	boost::system::error_code ec;
	boost::asio::write(conn_socket_, boost::asio::buffer(valid.data(), valid.size()));

	if(!ec){
		DataWrite();
	}
	else{
		conn_socket_.close();
	}
}


//Let the Client know that it's the end of this round of packets in the write queue, more will be added if another request is sent
void ServerConnection::NotifyEnd()
{
	//std::cout << "sending AsyncNotifyEnd value: " << '0' <<std::endl;
	std::vector<char> valid(1, '0');
	boost::system::error_code ec;
	boost::asio::write(conn_socket_, boost::asio::buffer(valid.data(), valid.size()));
	if(!ec){
		//Go back to data read
		//async_read();										
		}
	else{
		conn_socket_.close();
	}
 
}

void ServerConnection::FlagWrite(vector<char> FlagMsg){
	//Write one byte to the client dentoing if more data is going to be sent or no data.
	#ifdef DEBUG
		std::cout << "sending FlagMsg value: " << FlagMsg[0] <<std::endl;
	#endif
	boost::asio::write(conn_socket_, boost::asio::buffer(FlagMsg.data(), FlagMsg.size()));
}


void ServerConnection::on_read(boost::system::error_code ec, std::size_t msg_len)
{
	//std::cout << "In function On read\n";

	if(!ec){
		streambuf.consume(msg_len);
		//Have we sent one Flag yet?
		bool SendOne = false;
			//std::cout << "Got past while loop for data\n";
		while(1){
			if(FlagPost(SendOne)){
				//Returned one we are clear to send some data
				//#ifdef DEBUG
					//std::cout << "FlagPost returned True!\n";
				//#endif
				//This point the server either sent an f or a 1 if it's 1 need to postdata
					DataPost();		//Write Data here
					//Completed data post reset flag post flag
					//Go back up and post a flag if we have more data if we have more data
					//If we don't we'll send an invalid flag, this should cause the client to 
					//continue to wait for a datapost		
					break;
				}
				else{
					//Returned false we are either done or waiting on data
					//If we are done break because we already sent the f
					if(server_finished_flag_){
						break;
					}
					//Loop until we see data avaliable to send because we are not done
					continue;
					//break;
				}
			}
		#ifdef DEBUG
			std::cout<<"leaving flagpost loop!\n";
		#endif
		async_read();		//Go back to read and wait for ping
	}
	else
	{
		conn_socket_.close(ec);
	}
}

void ServerConnection::on_write(boost::system::error_code ec, std::size_t msg_size)
{	
	
	//std::cout << "In on write\n";
	if(!ec){
		write_queue_.pop();
		if(!write_queue_.empty()){
			//std::cout << "Data left in queue send sync Packet|\n";
			SyncPacketWrite();						
		}
		else{
			//std::cout <<"data is gone\n";
			NotifyEnd();
			//Write queue is empty go back and wait for input from client.
		}	
	}
	else
	{
		conn_socket_.close(ec);
	}
	#ifdef DEBUG
		std::cout << "leaving on_write callback\n";
	#endif
}

	

void ServerConnection::DataPost()
{
	//if(write_queue_.size() == 0){
		
		//live_data_ = SafePop();
		//std::cout<<"going to write: "<<LiveData[0]<<"\n";
		//std::vector<char> valid_buf;
		//Check if we are going to send anything
		//valid_buf.push_back((live_data_.size() > 1) ? '1' : '0');
    	//write_queue_.push(valid_buf);
    	//write_queue_.push(live_data_);
	//}
	vector<char> LiveData;
	server_lock_.lock();
	// while the server msg queue is empty and less than 5 messages are still in the queue
	//FIXME: Magic number, not important for functionality just dont want to have the write queue expand indef
    while( !server_msg_queue_.empty() && write_queue_.size() <= 128)	
	{ 
		#ifdef DEBUG
			//std::cout << "\tserver_msg_queue_ size is : "<< server_msg_queue_.size();
			//std::cout <<"\n\tWrite_queue_ size is : " << write_queue_.size() << std::endl;
		#endif
		write_queue_.push(server_msg_queue_.front());
        server_msg_queue_.pop();
    }
	server_lock_.unlock();
	//std::cout << "writing data from data queue\n";
	DataWrite();	//Write the data in the Msg Queue
}



bool ServerConnection::FlagPost(bool& SendOne)
{
	vector<char> LiveData(1, '1');
	#ifdef DEBUG
		//std::cout << "In FlagPost" << std::endl;
	#endif
	server_lock_.lock();
	if(!server_msg_queue_.empty() || !write_queue_.empty()){
		//std::cout << "msg_queue is not empty there is data to send\n";
		// write a 1
		server_lock_.unlock();
		if(!SendOne){
			FlagWrite(LiveData);
			SendOne = true;
		}
		return true;
	}
	else{
		#ifdef DEBUG
			//std::cout << "Finding out if we send a 0 or f\n";
		#endif
		//msg_queue is empty send a zero if we are not done
		//	send an f if we are
		server_lock_.unlock();
		if(server_finished_flag_){
			LiveData[0] = 'f';
			if(!SendOne){
				FlagWrite(LiveData);
				SendOne = true;
			}
			return false;
		}
		else{
			//We are not finished but we have no data to send
			LiveData[0] = 'w';
			
		}
		if(!SendOne){		
			FlagWrite(LiveData);
			SendOne = true;
		}
		return false;
	}
}


tcp::socket& ServerConnection::GetSocket(void)
{
	return conn_socket_;
}


