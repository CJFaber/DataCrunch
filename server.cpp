#include <iostream>
#include <fstream>
#include <limits>

#include "CrunchServer.hpp"



int main(int argc, char *argv[])
{
 
	//vector<char> testvalue1 (DC_MESSAGE_SIZE, 'A');
	//vector<char> testvalue2 (DC_MESSAGE_SIZE, 'C');


	if (argc != 2)
  	{
    	std::cerr << "Usage: ./server.out <input_file>" << std::endl;
     	return 1;
    }
 	vector<char> message(DC_MESSAGE_SIZE, 0);
	char ctrl;
	std::ifstream f;
	f.open(argv[1]);
	f.ignore( std::numeric_limits<std::streamsize>::max() );	//Run to the end of the file
	size_t inp_size = (size_t)f.gcount();
	f.clear();
	f.seekg(0,std::ios_base::beg);

	std::cout << "size of " << argv[1] << " is: " << inp_size << " bytes.\n";

	std::cout << "Starting Server\n";
	
	CrunchServer serv;
	std::cout << "Attempting to start the server io_context run in another thread\n";
	serv.Run();
	char tst;
	std::cout << "Server started, sending hello message, press enter to start writing to queue\n";
	std::cin.get(tst);
	
	while(!f.eof()){
		f.read(message.data(), message.size());
		message.resize(DC_MESSAGE_SIZE, 0);
		serv.LoadData(message);
		//std::cout<<"Loaded message of size: " << message.size() << std::endl;
		//for (auto i = message.begin(); i != message.end(); ++i){
		//	std::cout << *i;
		//}
		message.clear();
		message.resize(DC_MESSAGE_SIZE);
		//std::cin.get(tst);
	}

	f.close();
	//std::cout<<"---\nfinished sending file press any key to post end messag\n---\n";
	//std::cin.get(tst);
	serv.PostEndMessage();	
	//std::cin.get(tst);
	//serv.PostEndMessage();
	std::cout << "Posting end message. Enter any key to stop server\n";
	std::cin.get(tst);
	serv.Stop();		
	return 0;
}
