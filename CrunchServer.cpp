#include "CrunchServer.hpp"

//Server constructor
CrunchServer::CrunchServer(void):
	server_acceptor_(server_context_, tcp::endpoint(tcp::v4(), DC_PORT)),
	server_work_(boost::asio::make_work_guard(server_context_))
{
	finished_flag_ = false;
	server_acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
	server_acceptor_.listen();
	StartAccept();
}

//Load data from main thread
void CrunchServer::LoadData(vector<char> Msg)
{
	#ifdef DEBUG
		std::cout << "In LoadData\n";	
	#endif
	std::unique_lock<std::mutex> lock(message_lock_);		
	message_queue_.push(Msg);
	message_sig_.notify_all();
}

//Set the finished flag and signal incase the socket thread is asleep
void CrunchServer::PostEndMessage(void)
{
	std::unique_lock<std::mutex> lock(message_lock_);
	finished_flag_ = true;
	message_sig_.notify_all();
}

//Starts the server context work queue
void CrunchServer::Run(void)
{
	server_thread_ = std::thread([this](){server_context_.run();});
}

//Stops server context work queue and waits to join with the thread
void CrunchServer::Stop(void)
{
	server_context_.stop();	//Will stop a read if wating on one
	server_thread_.join();
}

//Accepts an incomming connection and creates a server connection
void CrunchServer::StartAccept(void)
{
	server_acceptor_.async_accept(
		[this](boost::system::error_code ec, tcp::socket server_socket)
		{
			if(!ec){
				std::make_shared<ServerConnection>(std::move(server_socket), message_queue_, message_lock_, message_sig_, 
													finished_flag_) -> Start();
			}
			StartAccept();
		});
}

//////////////////////
// End Server class //
/////////////////////

//Server Connection Constructor
ServerConnection::ServerConnection(tcp::socket server_socket, queue<vector<char>>& server_queue, std::mutex& mutex, 
									std::condition_variable& cond_var, bool& fin_flag) : 
	conn_socket_(std::move(server_socket)), 
	server_msg_queue_(server_queue), 
	server_msg_lock_(mutex), 
	server_msg_sig_(cond_var),
	Delim_('0'), 
	server_finished_flag_(fin_flag),
	queue_empty_flag_(true)
{
	
	#ifdef DEBUG
		std::cout << "Starting server connection\n";
	#endif
	//Empty Constructor

}

//Calls read ping, will happen after the server connection is created
void ServerConnection::Start()
{
	//As soon as we get a ping we'll enter the sending loop
	ping_read();
}


//Read until we get a '0' from the client which will trigger us to send one ping packet and everything in write queue datagram
void ServerConnection::ping_read()
{
	boost::asio::async_read_until(conn_socket_, streambuf, "0", 
									[self = shared_from_this()] (boost::system::error_code ec, std::size_t msg_len)
									{
										self->startup_on_ping(ec, msg_len);
									});
}

//Writes the live_data_ vector to the client
void ServerConnection::DataWrite()
{	
	#ifdef DEBUG
		std::cout<<"In Data Write Going to write Data to client " << std::endl;
	#endif
	boost::system::error_code ec;
	//This function will bock until finished 
	boost::asio::write(conn_socket_, boost::asio::buffer(live_data_,(sizeof(char) * live_data_.size())));
				
}

//Writes a flag to the client given a flag vector
void ServerConnection::FlagWrite(vector<char> FlagMsg){
	//Write one byte to the client dentoing if more data is going to be sent or no data.
	#ifdef DEBUG
		std::cout << "sending FlagMsg value: " << FlagMsg[0] <<std::endl;
	#endif
	//This function will bock until finished
	boost::asio::write(conn_socket_, boost::asio::buffer(FlagMsg.data(), FlagMsg.size()));
}

//Callback for read_ping, start the main loop function, ServeData()
void ServerConnection::startup_on_ping(boost::system::error_code ec, std::size_t msg_len)
{
	//std::cout << "In function On read\n";

	if(!ec){
		#ifdef DEBUG
			std::cout << "In Function starup_on_ping jumping to serve data!\n";
		#endif
		streambuf.consume(msg_len);
		//Jump into send loop
		ServeData();
	}
	else
	{
		conn_socket_.close(ec);
	}
}

	
//Main loop of the server thread
void ServerConnection::ServeData()
{
	// 1 - Send flag (FlagPost) if returns false, return call read ping
	// 2 - Send Data, everything you've got.
	//	This will loop and send continously as long as there's data, wait if there isn't. 

	// 	The client should know that once it sends the ping to loop back and forth between status
	// 	flag reads and then message reads, when pinged again after sending the finished flag
	// 	it should only send the finished flag.

	while(1){
		//Got a true back from flag post, try to send data
		#ifdef DEBUG
			std::cout << "In while loop for FlagPost return true\n";
		#endif
		std::unique_lock<std::mutex> data_wait(server_msg_lock_);
		while(server_msg_queue_.empty() ){			//Prevent incorrect waits, if there's already data continue.
			if(server_finished_flag_) break;		//Break out of while when server finished flag is set and the queue is empty
			#ifdef DEBUG
				std::cout<<"No data to serve and not finished, putting thread: " << std::this_thread::get_id() << " to sleep!\n";
			#endif
			server_msg_sig_.wait(data_wait);
			
		}
		queue_empty_flag_ = server_msg_queue_.empty();

	
		//If we own the lock we must have data to send pop it and unlock the lock
		// If we don't own the lock then we either have  AND/OR the server finished flag was set
		// 	If the queue is not empty but the finished flag is set we won't have locked the data_wait lock
		// 	However the same is true if the finished flag is set and the queue is empty.
		// 	Need to have a way to make sure that the queue is not empty before poping off the front, because we still want to send
		// 	items in the queue if they do indeed exist.

		if(!queue_empty_flag_){
			live_data_ = server_msg_queue_.front();
			server_msg_queue_.pop();

			//This seems kind of silly but I can't really think of another way to quickly relase the lock (correctly)
			//	We either move the FlagPost function before the unlock OR we let the lock go out of scope (end of while iteration)
			//	I would imagine that the check to see if the lock is owned is very quick (quicker than running the FlagPost funciton),
			//	but I am uncertian. This should however work for the time being. 

			if(data_wait.owns_lock()){
				data_wait.unlock();
			}
		}
		#ifdef DEBUG
			std::cout<<"Got data from msg_queue after condition\n";
		#endif

		//Do flag write, if we return false we sent an f and are done sending data so break while loop
		if(!FlagPost()) break;					

		//Write live_data_ to client with DataWrite.
		DataWrite();
		//Done with DataWrite
	}
	//Call ping read to start a new transaction
	// If we hit this for this version of the code when a new ping is sent 
	// we will just send the finish flag right away.
	ping_read();
}


//Decides what flag to send to the client before the message is sent.
bool ServerConnection::FlagPost()
{
	vector<char> LiveData(1, 't');
	#ifdef DEBUG
		std::cout << "In FlagPost" << std::endl;
	#endif	
	if(server_finished_flag_ && queue_empty_flag_){	
		LiveData[0] = 'f';
		#ifdef DEBUG
			std::cout << " sending a \"" << LiveData[0] << "\"\n" << std::endl;
		#endif
		FlagWrite(LiveData);
		return false;
	}
	//else{
		//We are not finished but we have no data to send
		//Send a wait flag
		//LiveData[0] = 'w';
	//}
	//Will send a t or w, shouldn't matter.
	#ifdef DEBUG
		std::cout << " sending a \"" << LiveData[0] << "\"\n" << std::endl;
	#endif
	FlagWrite(LiveData);
	return true;
}

//returns the socket of the server connection
tcp::socket& ServerConnection::GetSocket(void)
{
	return conn_socket_;
}


//Time Stamp things
//
// Prints a time stamp to the console using std::chrono and
// a high resolution timer in microseconds
////////////////////////////////////////////////////////////////

void TimeStamp(void){
	const std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::microseconds> now = 
		std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now());
	
	const std::chrono::high_resolution_clock::duration now_since_epoch = now.time_since_epoch();
	
	std::cout << std::chrono::duration_cast<std::chrono::microseconds>(now_since_epoch).count() << std::endl;
}
