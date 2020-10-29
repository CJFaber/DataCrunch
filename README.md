# DataCrunch
Boost asio client server

 Compile using g++
 - requires Boost lib version > 1.71
 - std=c++11

Example programs 
  - server.out:
```
g++ server.cpp CrunchServer.cpp -o server.out -lboost_system -lboost_thread -lpthread
```
  - client.out:
```
g++ client.cpp CrunchClient.cpp -o client.out -lboost_system -lboost_thread -lpthread
```
