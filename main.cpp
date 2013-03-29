#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <dirent.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "head.h"
#include <vector>
#include <semaphore.h>
using namespace std;
/**
 myhttpd web sever
 **/

//thread pool
struct exec_thread* threadpool;
//scheduler
pthread_t scheduler;
//request queue
vector<Request*> ready_queue;

pthread_mutex_t queue_lock;//for request ready queue
pthread_mutex_t lock;
sem_t sem_q;//for schedduler start
sem_t sem_ths;//for scheduler to choose exec_thread
pthread_cond_t cv;

int debug=0;
int port=8080;//default port

char* logFile=NULL;
char* sDir=NULL;
int sleep_time=60; //scheduler sleep time
int thread_num=4;//default thread_num in the thread_pool
int sche_flag=1;//default scheduler

int main(int argc,char** argv){
	opterr=0;
	int options;
	while((options=getopt(argc,argv,"l:p:r:t:n:s:dh"))!=-1){
		switch(options){
			case'd':
			{
			    debug=1;
				
				thread_num=1;
				break;
			}
			case'h':
			{
			    Info();
				//print options then exit
			    exit(0);
				break;
			}
			case'r':
			{
			    sDir=optarg;
				break;
			}
			case't':
			{
			    sleep_time=atoi(optarg);
				break;
			}
			case'l':
			{
			    logFile=optarg;
				break;
			}
			case'p':
            {
                port=atoi(optarg);
                break;
            }
			case'n':
			{
			    thread_num=atoi(optarg);
				break;
			}
			case's':
			{
			    string sche=string(optarg);
                if(sche=="SJF"){
                    sche_flag=2;
                }
				else if(sche == "FCFS"){
					sche_flag = 1;
				}
				else{
					printf("Unknown schedule algorithm!\n");
					exit(0);
				}
				break;
			}
			default:
			printf("Unknown Options!");
		}
	}
	//deamonize
	if(!debug){
	    pid_t pid, sid;
        if ((pid=fork())<0){
			printf("eroor\n");
			exit(0);
		}else if (pid!=0) {
			printf("deamonize thread pid = %d\n",pid);
			fflush(stdout);
            exit (0);
        }
			
        umask(0); 
		sid = setsid();
        if(sid < 0){
			printf("set sid fail\n");
			exit(0);
		}// session leader
        if(chdir("./")<0){
			printf("change chdir fail\n");
			exit(0);
		}


    }
	//socket starts
	struct sockaddr_in saddr;
	int server_socket,client_socket;         

	server_socket=socket(AF_INET,SOCK_STREAM,0);

	
	if(server_socket==-1){ 
			printf("socket creating error...\n");
		exit(0);
	}

	saddr.sin_family=AF_INET;
	saddr.sin_addr.s_addr=INADDR_ANY;
	saddr.sin_port=htons(port);
	bzero(&(saddr.sin_zero),8);

	if(bind(server_socket,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
		printf("binding error: this port is in use!\n");
		exit(0);
	}

	if(listen(server_socket,20)==-1){ 		printf("listen error...\n");
		exit(0);
	}

	threadpool=new exec_thread[thread_num];

	
	int i;

	pthread_cond_init(&cv,NULL);
	pthread_mutex_init(&lock,NULL);
	pthread_mutex_init(&queue_lock,NULL);

	sem_init(&sem_q,0,0);
	sem_init(&sem_ths,0,thread_num);


	//create threads
	for(i=0;i<thread_num;i++){
		//the locker here is used for synchronize the threadId
	    pthread_mutex_lock(&lock);
	    pthread_create(&(threadpool[i].thread),NULL,run,&i);
	    threadpool[i].isFree=1;
	    pthread_cond_wait(&cv,&lock);
	    pthread_mutex_unlock(&lock);
	}
	//create schedule thread
	if(sche_flag==2){
	    pthread_create(&scheduler,NULL,SJF_scheduler,NULL);
	}else{
	    pthread_create(&scheduler,NULL,FCFS_scheduler,NULL);
	}
	printf("Server started...\n");
	fflush(stdout);

	//accept income connections
	char buffer[1024]; 
	while(1){
		client_socket=accept(server_socket,NULL,NULL);
		if(client_socket==-1){
			printf("client socket error...");
		}
		char* recv_time=Time();//get receive time for dubug mode and logging
        fflush(stdout);
		
		struct Request *req;
		recv(client_socket,buffer,1024,0);
		

		fflush(stdout);

		req=http_analyzer(buffer);
		req->client_socket=client_socket;
		//if in debug model or logfile activated,get the client address and recv_time
		if(debug==1||logFile){
				
		    socklen_t r_length = sizeof(struct sockaddr_in);

            struct sockaddr_in r;
            req->recv_time=recv_time;//record receive time

            if(getpeername(client_socket, (struct sockaddr*)&r, &r_length) == 0){
					//get ip addr
                req->ip = inet_ntoa(r.sin_addr);
            }
		}
		pthread_mutex_lock(&queue_lock);
		ready_queue.push_back(req);//push the request into the request ready queue
		sem_post(&sem_q);//one request is accepted, scheduler can start
        pthread_mutex_unlock(&queue_lock);
	}
}




/**execution thread function**/
void* run(void* id){
        pthread_mutex_lock(&lock);
        int thread_id=*(int*)id;
		//sem_t *sem;
		//in MAC OS, only sem_open can be used
		/*
		if((sem =sem_open("sem",O_CREAT,0,0))== NULL){
                cout<<"Thread ["<<thread_id<<"] semephore init failed..."<<endl;
        }
		else{
			threadpool[thread_id].sem= *sem;
		}
		*/
        if(sem_init(&(threadpool[thread_id].sem),0,0)!=0){
                printf("Thread %d semephore init failed...",thread_id);
        }
        pthread_cond_signal(&cv);//after using the pointer "id",wake up main thread.
        pthread_mutex_unlock(&lock);//after
        char buffer[1024];
        //keep looking for job
        while(true){
            sem_wait(&(threadpool[thread_id].sem));//wait for scheduling
            if(threadpool[thread_id].isFree==0){//this thread is scheduled
                struct Request* r=threadpool[thread_id].req;
                if((r->contentLength>0)){//requested file is exsit,just response
                        
                        string rHead;
						printf("response....\n");
                        r->status=200;//200 - find file

                        getResponse(r,rHead);
                        //Head Response just send back response
						if(r->requestType==1){
							if(r->isDir==1){
								//if it is a directory then generate fileindex page
                                char* dir_index=generateIndex(r);//generate html first so that the actrual contentlength can be modified
                                // response head 
                                const char*res=rHead.c_str();
                                send(r->client_socket,res,strlen(res),0);
								//generate fileindex html page
                                send(r->client_socket,dir_index,strlen(dir_index),0);
								free(dir_index);
							}
							else{
								//send file
                                const char*resp=rHead.c_str();
                                send(r->client_socket,resp,strlen(resp),0);
								
                                int lenth=1;
                                /**send file data**/


                                int fh=open(r->requestDir,O_RDONLY);

                                while(lenth>0){
                                    lenth=read(fh,buffer,1024);
                                    if(lenth>0){
                                        send(r->client_socket,buffer,lenth,0);//continueous sending the data
										//sleep(1000);
                                    }
                                }
                                close(fh);
                            }
                        }

                        else if(r->requestType==2){
                            const char* resp=rHead.c_str();
                            send(r->client_socket,resp,strlen(resp),0);
                        //Get Response send back response, and open the requested file
	
                        }               
				}else{
						//request not found
                        
                        r->status=404;
						string rHead;
                        getResponse(r,rHead);//404 NOT FOUND
                        const char* resp=rHead.c_str();
                        send(r->client_socket,resp,strlen(resp),0);
                }

                //close the socket
                close(r->client_socket);
                if(logFile!=NULL||debug==1){
                    logging(r,debug,logFile);
                }

                threadpool[thread_id].isFree=1;
                free(r);
                sem_post(&sem_ths);//this thread is free, notify the scheduler

            }
        }
	return NULL;
}


//analyse http request
struct Request* http_analyzer(char* p){
		char split_line[]="\n";

        int flag=0;

        
        struct Request* req=(struct Request*)(malloc(sizeof(struct Request)));
		char* m=strtok(p,split_line);

        if(debug==1||logFile){//if in debug model or logfile activated,record the first line of request
            char* n = (char*)malloc(sizeof(char)*strlen(m));
			int len = strlen(m);
            strncpy(n,m,len-1);//get rid of "\n" in the last of string
            *(n+len-1)='\0';//add end
            req->req_head=n;
        }

        char split_word[]=" \n";
		char* tok=strtok(p,split_word);


        while(tok!=NULL){
            if(flag==0){
				//first tok is about request method
                string head=string(tok);
                if(head=="HEAD"){
					req->requestType=2;
                }
				else if(head=="GET"){
                    req->requestType=1;
                }
             }
            //file directory of request
            else if(flag==1){
					char* dir = (char*)malloc(sizeof(char)*strlen(tok));
                    strcpy(dir,tok);//file directory


                    //analyse the file path
                    analyseUrl(dir,sDir,req);
                    if(req->isDir==0){
						//if request a file,get the file length
                        ifstream instream;
                        instream.open(req->requestDir,ios::in|ios::binary);
                        if(!instream){
                            req->contentLength=-1;
                        }else{
                            instream.seekg(0,ios::end);
                            int f_len=instream.tellg();
							//contentlength can be used in SJF, or under debug mode, or log,or decide wether a file is find
                            req->contentLength=f_len;    
							}
                        instream.close();
                    }else if(req->isDir==1){
						//check if the diretory exists
                        DIR *direcroy=NULL;
                        direcroy=opendir(req->requestDir);
                        if(direcroy==NULL){
                            req->contentLength=-1;
                        }
                        else{
							//if direcory is found,then set the contentlength to 200 to define it
                            req->contentLength=200;
                        }
                    }else{//unknow Request
                        req->contentLength=-1;
                    }
            }
            else if(flag==3){
				//dont need other rquest information, just break the loop
				if(string(tok)!="HTTP/1.0"||string(tok)!="HTTP/1.1"){
                    req->contentLength=-1;
                }
                break;
            }
            flag++;
            tok=strtok(NULL,split_word);

        }
        return req;
}
/**SJF**/
void* SJF_scheduler(void* p){
    sleep(sleep_time);
    while(true){
        sem_wait(&sem_q);
        if(ready_queue.size()!=0){
            /**get reqeust from the ready queue**/
            int index=0,i=0;//to store the index of the shortest request
            struct Request* req;
            pthread_mutex_lock(&queue_lock);//lock
			//the first job is the default job to chose to run,there is atleast 1 job in the queue,when scheduler is waked up
            req =ready_queue[0];
			//if(ready_queue.size()==1){
			//	ready_queue.erase(ready_queue[0]);
			//}
			//find the shortest job
            for(unsigned int i=0;i<ready_queue.size();i++){
                    if(req->contentLength > ready_queue[i]->contentLength){
                        req=ready_queue[i];
                        index=i;
                    }
            }
            //after getting the shortest job in the queue,then delete it
            ready_queue.erase((ready_queue.begin()+index));
            pthread_mutex_unlock(&queue_lock);
            /**assign the request to a certain execution thread**/
            sem_wait(&sem_ths);//wait if there's no idle threads,actually we can spin here too.
            for(i=0;i<thread_num;i++){
                if(threadpool[i].isFree==1){
                    if(debug==1||logFile){
							//record the assign time
                        req->asgn_time=Time();
                    }
					threadpool[i].isFree=0;
                    threadpool[i].req=req;

					//wake up the free thread.
                    sem_post(&(threadpool[i].sem));
                    break;
                }
            }
        }
    }
	return NULL;
}

/**FCFS**/
void* FCFS_scheduler(void* p){
    cout<<"Default scheduler: FCFS, starts after "<<sleep_time<<"seconds!<<<<"<<endl;
    sleep(sleep_time);
    while(true){
        sem_wait(&sem_q);
        if(ready_queue.size()!=0){
            int i=0;
            //when access to the ready queue, we need a lock

            pthread_mutex_lock(&queue_lock);
			//get the head of the queue,FCFS,then delete it
            struct Request* req =ready_queue[0];
            ready_queue.erase(ready_queue.begin());
			//dont forget to free the lock
			pthread_mutex_unlock(&queue_lock);
            //there are only 4 threads in the pool,so use a sem to control
			//if there no free thread, just wait
            sem_wait(&sem_ths);

            for(i=0;i<thread_num;i++){
                if(threadpool[i].isFree==1){
                    
                    if(debug == 1||logFile){
						//record the assgn time
                        req->asgn_time=Time();
                    }
					threadpool[i].isFree=0;
                    threadpool[i].req=req;
					//wake up the selected thread
                    sem_post(&(threadpool[i].sem));
                    break;
                }
            }
        }
    }
	return NULL;
}

//parse the file path
void analyseUrl(char* path,char* sDir,struct Request* r){
    char* dir=NULL;

    char* type=(char*)malloc(sizeof(char)*9);
    if(strlen(path)==1){//only "/" after port,such as "8080/"
        r->isDir=1;//it indicate a dir
        if(sDir==NULL){
			//server local dir
            dir=(char*)malloc(sizeof(char)*2);
			//current dir
            strcpy(dir,"./");

        }else{
            dir=sDir;
        }
        //return 1;//is directory
    }else if(!sDir){// no ~, no sDir
		//get the file path after "/"
        dir=1+path;
	}else if(*(path+1)=='~'){
		
        string new_dir;
		//get the server homedir path
        struct passwd* pw;
        pw=getpwuid(getuid());
		//get pw info,including home directory

        if(strlen(path)==2){
			//if there no further path after the /~, then return homedir+/myhtpd/,as mentionned in PROTOCOL part
            new_dir=string(pw->pw_dir)+"/myhttpd/";
        }else{
			//else append the rest path
            new_dir=string(pw->pw_dir)+"/myhttpd/"+string(path+2);
        }
        dir=(char*)malloc(sizeof(char)*new_dir.size());
		//printf("%s\n",dir);
        strcpy(dir,new_dir.c_str());
    }else{//server directory sepecified
        string sDir=string(path+1)+string(sDir);
        dir=(char*)malloc(sizeof(char)*sDir.size());
        strcpy(dir,sDir.c_str());
    }
	//9 means the max length of the string of content-type: "test/html" or "image/..."

    //do analyse the type
    if(*(dir+strlen(dir)-1) == '/'){
		//if it is derecory, try to find index.html in this dir
        
        struct stat* buff=(struct stat*)malloc(sizeof(struct stat));
		string f_path= string(dir)+"index.html";

		//stat(), similar to the open() to check wether certain file is exit or not, the first argument is the filepath
		int result = stat(f_path.c_str(),buff);
        if(result ==0){
            char* f= (char*)malloc(strlen(f_path.c_str())*sizeof(char));
            strcpy(f,f_path.c_str());
            r->isDir=0;
			r->requestDir=f;
            free(dir);
        }else{
			//index.html not found , set the direcory, finally we will see a fileindex page
            r->requestDir=dir;
			r->isDir=1;
        }

        strcpy(type,"text/html");
        r->contentType=type;
        free(buff);
		//contentType has no sense when the path indicates a dir and there is no index.html, but anyway, just set it to 'test/html'

    }else{
		//if request a file, it can be text or some other type file, or image
        r->requestDir=dir;
		r->isDir=0;
        char* tep = (char*)malloc(sizeof(char)*strlen(dir));
        strcpy(tep,dir);
        char* tok=strtok(tep,".");
		//find the suffix
        tok=strtok(NULL,".");
        //file without suffix will be ignored
        if(tok==NULL){
            
            free(tep);
			r->isDir=-1;
            return;
        }
        string s=string(tok);
        if(s=="gif"||s=="png"||s=="bmp"||s=="jpg"){
            string step="image/"+s;
            strcpy(type,step.c_str());
        }
		else if(s=="htm"||s=="html"||s=="txt"){
            strcpy(type,"text/html");
        }
		
        free(tep);
		r->contentType=type;

    }
}














