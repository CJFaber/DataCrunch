#include <iostream>
#include <fstream>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include "CrunchClient.hpp"

using boost::asio::ip::tcp;

int main(int argc, char* argv[])
{
	std::ios init(NULL);
	std::fstream fs;
	fs.open("./Out.fasta", std::ios_base::binary | std::ios_base::out);
	init.copyfmt(std::cout);
	if (argc != 3)
  	{
    	std::cerr << "Usage: client <server_ip_addr> <port>" << std::endl;
     	return 1;
    }
 	//vector<char> message;
	char ctrl;
	CrunchClient client(argv[1], argv[2]);
	client.Run();
	std::cout << "starting for loop" << std::endl;
	while(1){
		std::cout<<"\nStarting while loop: press enter to query for vector, or press any key followed by enter to quit\n";
		if(std::cin.get() != '\n'){
			break;
		}
		std::vector<char> message = client.GetData();
		//message.shrink_to_fit();
		int x = 0;
		std::cout<<"return\n";
		

		if(message.empty()){
			std::cout << "Got empty message\n" <<std::endl;
			continue;
		}		
		//if(message[0] == 'f'){
		if(!message.empty() && message.at(0) == 'f'){
			std::cout<<"Got an f!\n";
			break;
		}
		else{

		
			for (auto i = message.begin(); i != message.end(); ++i){
				x++;
			//std::cout << std::hex << (0xFF & (*i));
			//std::cout << (*i);
				fs << (*i);
			//std::cout << ", ";
			}
		}
		
		std::cout << "\n\t Size of mesage is: " << message.size() << std::endl;
		std::cout << "\n\t For loop did: " << x <<std::endl;

		//std::cout.copyfmt(init);

	}
	std::cout<<"leaving while loop?\n";
	client.Stop();
  return 0;
}
