#include <pthread.h>
#include <string>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <semaphore.h>


using namespace std;

//exection thread structure
struct exec_thread{
    pthread_t thread;//thread id
    struct Request* req;//present request
    sem_t sem;//semophore
    int isFree;//flag
};

//http request structure
struct Request{
	//for debuging info and logging
    
	char* req_head;
	//remote IP address
	char* ip;
	//the time the request was reveived by the queueing thread
	char* recv_time;     
	//the time the request was scheduled
	char* asgn_time;    
    //200,404 or others
	int status;  	
	//size of the response is  "content-length"

	//client socket id
    int client_socket;
	//GET or HEAD
	int requestType;
	const char* requestDir;
    int contentLength;//the length of requested file, 0  for HEAD, -1 for invalid request, others for GET.
    char* contentType;//text/html or image/gif
    int isDir;// 1  for directory,0  for file, -1 for invalid URL
};

void* run(void* id);
struct Request* http_analyzer(char* p);
void analyseUrl(char* url,char* serverDir,struct Request* r);

//scheduler
void* FCFS_scheduler(void* p);
void* SJF_scheduler(void* p);

char* Time();
char* lastmodifyTime(char* d );
void getResponse(struct Request* req,string &res);
void logging(struct Request* req,int debug,char* logfile);
char* generateIndex(struct Request* req);
void Info();



