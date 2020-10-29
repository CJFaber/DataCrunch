#include "CrunchServer.hpp"

CrunchServer::CrunchServer(void):
	server_acceptor_(server_context_, tcp::endpoint(tcp::v4(), DC_PORT)),
	server_work_(boost::asio::make_work_guard(server_context_))
{
	server_acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
	server_acceptor_.listen();
	StartAccept();
}

void CrunchServer::LoadData(vector<char> Msg)
{
 	message_lock_.lock();	
	message_queue_.push(Msg);
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
				std::make_shared<ServerConnection>(std::move(server_socket), message_queue_, message_lock_)->Start();
			}
			StartAccept();
		});
}

ServerConnection::ServerConnection(tcp::socket server_socket, queue<vector<char>>& server_queue, boost::mutex& mutex) : 
	conn_socket_(std::move(server_socket)), server_msg_queue_(server_queue), server_lock_(mutex), Delim_('0')
{

}

void ServerConnection::Start()
{
	async_read();
}

void ServerConnection::async_read()
{
	boost::asio::async_read_until(conn_socket_, streambuf, "0", 
									[self = shared_from_this()] (boost::system::error_code ec, std::size_t msg_len)
									{
										self->on_read(ec, msg_len);
									});
}

void ServerConnection::async_write()
{
	std::cout<<"Going to write: " << write_queue_.front().size() * sizeof(char) << std::endl;
	boost::asio::async_write(conn_socket_, boost::asio::buffer(write_queue_.front().data(),
																	(sizeof(char) * write_queue_.front().size())),
								[self = shared_from_this()](boost::system::error_code ec, std::size_t msg_size)
								{
									self->on_write(ec, msg_size);
								});
}

void ServerConnection::on_read(boost::system::error_code ec, std::size_t msg_len)
{
	if(!ec){
		streambuf.consume(msg_len);
		DataPost();
		std::cout<<live_data_.size()<<std::endl;
		live_data_.clear();
		async_read();
	}
	else
	{
		conn_socket_.close(ec);
	}
}

void ServerConnection::on_write(boost::system::error_code ec, std::size_t msg_size)
{
	if(!ec){
		write_queue_.pop();
		if(!write_queue_.empty()){
			async_write();
		}
		//Maybe in the future we would like to go back and write more
	}
	else
	{
		conn_socket_.close(ec);
	}
}

	

void ServerConnection::DataPost()
{
	live_data_ = SafePop();
	//std::cout<<"going to write: "<<LiveData[0]<<"\n";
	std::vector<char> valid_buf;
    valid_buf.push_back((live_data_.size() > 1) ? '1' : '0');
    write_queue_.push(valid_buf);
    write_queue_.push(live_data_);
	async_write();
}

vector<char> ServerConnection::SafePop()
{
	vector<char> LiveData;
	server_lock_.lock();
	if(!server_msg_queue_.empty()){
		LiveData = server_msg_queue_.front();
		server_msg_queue_.pop();	
	}
	else{
		LiveData.push_back('0');
	}
	server_lock_.unlock();
	return LiveData;
}


tcp::socket& ServerConnection::GetSocket(void)
{
	return conn_socket_;
}


