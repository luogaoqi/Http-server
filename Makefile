myhttpd: main.o options.o
	g++ -o myhttpd -pthread main.o options.o 
main.o: main.cpp head.h
	g++ -c -Wall main.cpp
options.o: options.cpp head.h
	g++ -c -Wall options.cpp
clean:
	rm main.o options.o

