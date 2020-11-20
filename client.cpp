#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include "CrunchClient.hpp"

using boost::asio::ip::tcp;

int main(int argc, char* argv[])
{

	if (argc != 3)
  	{
    	std::cerr << "Usage: client <server_ip_addr> <port>" << std::endl;
     	return 1;
    }
 	vector<char> message(DC_MESSAGE_SIZE, 'a');
	char ctrl;
	CrunchClient client(argv[1], argv[2]);
	client.Run();
	std::cout<<"Trying to get Hello Buf...\n";
	message = client.GetData();
	for (auto i = message.begin(); i != message.end(); ++i){
		std::cout << *i;
	}
	std::cout << std::endl;
	while(1){
		std::cout<<"\nStarting while loop: press enter to query for vector, or press any key followed by enter to quit\n";
		if(std::cin.get() != '\n'){
			break;
		}
		message = client.GetData();
		std::cout<<"return\n";
		if(message.empty()){
			std::cout << "Got empty message\n" <<std::endl;
		}
		if(message[0] == 'f'){
			std::cout<<"Got an f!\n";
			break;
		}
		for (auto i = message.begin(); i != message.end(); ++i){
			std::cout << *i;
		} 
	}
	client.Stop();
  return 0;
}
