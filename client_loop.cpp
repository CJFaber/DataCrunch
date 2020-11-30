#include <iostream>
#include <omp.h>
#include <fstream>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include "CrunchClient.hpp"

using boost::asio::ip::tcp;

int main(int argc, char* argv[])
{

	double total_time = 0;
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
	//message = client.GetData();
	//for (auto i = message.begin(); i != message.end(); ++i){
	//	std::cout << *i;
	//}
	//std::cout << std::endl;
	std::ofstream f("Clientout.txt");
	std::cout<< "Press enter to start\n";
	std::cin.get();
	double start, end;
	while(1){
		//std::cout<<"\nStarting while loop: press enter to query for vector, or press any key followed by enter to quit\n";
		//if(std::cin.get() != '\n'){
		//	break;
		//}
		start = omp_get_wtime();
		message = client.GetData();
		end = omp_get_wtime();
		//std::cout<<"return\n";
		if(message.empty()){
			//std::cout << "Got empty message\n" <<std::endl;
			continue;
		}
		if(message[0] == 'f'){
			std::cout<<"Got an f!\n";
			break;
		}
		for (vector<char>::const_iterator i = message.begin(); i != message.end(); ++i){
			if(i == message.begin()){
				total_time += end - start;
			}
			if(*i != 0){
				f << *i;
			}
		} 
	}
	std::cout << "Total time elapsed: " << total_time << "s\n";
	client.Stop();
  return 0;
}
