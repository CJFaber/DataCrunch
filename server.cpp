#include "CrunchServer.hpp"



int main(int argc, char *argv[])
{
 
	vector<char> testvalue1 (DC_MESSAGE_SIZE, 'A');
	vector<char> testvalue2 (DC_MESSAGE_SIZE, 'C');
	
	CrunchServer serv;

	std::cout << "Attempting to write first test value to server\n";
	serv.LoadData(testvalue1);
	std::cout << "Success\n";

	
	std::cout << "Attempting to start the server io_context run in another thread\n";
	serv.Run();
	std::cout << "Attempting to write 2nd test value to server, waiting for usr input\n";
	char tst;
	std::cin.get(tst);
	serv.LoadData(testvalue2);
	std::cout<<"Success\n";
	std::cout << "enter any key to stop server\n";
	std::cin.get(tst);
	serv.Stop();		
	return 0;
}
