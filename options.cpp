#include "head.h"
#include <time.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <cstring>
#include <dirent.h>

using namespace std;

//last modify time
char* lastmodifyTime(const char* dir ){
    
    char* mtime=(char*)malloc(sizeof(char)*25);
	struct stat* buff=(struct stat*)malloc(sizeof(struct stat));

    stat(dir,buff);
	////
	//time_t st_mtime in struct *stat identifies the last modify time,
	//ctime: convert a time_t type time into string
    mtime=ctime(&(buff->st_mtime));
    return mtime;
}

/**HTTP Response Head
   include:
   status,date,server,last-modified,content-type,content-length
   this should be sent back with the socket
**/
void getResponse(struct Request* q,string &response){
    
    string server="Gaoqi's Web Server.\n";
	string date="Date:"+string(Time())+"\n";
	string s;
	//successful request

    switch(q->status){
        case 404:
            s="HTTP/1.0 404 NOT FOUND\n";
			response=s+date+server+"\n<html><h1>File Not Found</h1></html>";

            break;
		case 200:
            {
				s="HTTP/1.0 200 OK\n";	

				char con_len[10];
				string con_Type="Content-Type:"+string(q->contentType)+"\n";
				sprintf(con_len,"%d",q->contentLength);
				//the last head line should be separated by a blank line with the content.
				string conLength="Content-Length:"+string(con_len)+"\n\n";

				string lasdMod="Last-Modified:"+string(lastmodifyTime(q->requestDir));

				response=s+date+server+lasdMod+con_Type+conLength;
			}
            break;
    }
}

/**
 current time
**/
char* Time(){

    time_t timet;
    timet=time(NULL);
    char *format_time=(char*)malloc(sizeof(char)*30);
	struct tm* p;
    p=localtime(&timet);
    strftime(format_time,30,"[%d/%b/%Y:%T %z]",p);
    return format_time;
}

/**Generate Log information
   when in the debug mode, print log information
   when there is a logfile, then wirte the logfile
   logging info:
   IP,receive time,execution time,request head,status,content-length
**/
void logging(struct Request* r,int debug,char* ldir){
    string stat;
	//http 1.0
    switch(r->status){
        case 404:
			//bad request
            stat="404";
            break;

		case 200:
			//find file
            stat="200";
            break;
        default:
            stat="500";
    }
    if(debug==1){
		//print log information
        cout<<r->ip<<" - "<<r->recv_time<<" "<<r->asgn_time<<" \""<<r->req_head<<"\" "<<stat<<" "<<r->contentLength<<endl;
    }else{
		//there is a logfile
        
		string log;
		char content_lenth[10];

        sprintf(content_lenth,"%d",r->contentLength);

		//write logfile
        log=string(r->ip)+" - "+string(r->recv_time)+" "+string(r->asgn_time)+" \""+string(r->req_head)+"\" "+stat+" "+string(content_lenth)+"\n";

        ofstream outstream;
        outstream.open(ldir,ios::out|ios::app);
        outstream.write(log.c_str(),log.size());
        outstream.close();
    }
}

void Info(){
    cout<<"SERVER NAME\n"
        <<"       myhttpd\n\n"
        <<"SYNOPSIS\n"
        <<"       myhttpd [-d] [-h] [-p port] [-r dir] [-t time] [-n threadnum] [-s sched]\n\n"
        <<"DESCRIPTION"
        <<"    myhttpd is a simple web server.It binds to a given port on the given address and waits for incoming HTTP/1.0 requests.It serves content from the given directory.That is, any requests for documents is resolved relative to this directory.\n\n"
        <<"    -d\n"
        <<"           :Enter debugging mode.That is ,the server process will not daemonize,only accept one connection at a time and enable logging to stdout. Without this option, the web server runs as a daemon process in the background.\n\n"
        <<"    -h\n"
        <<"           :Print the usage of all the options and exit.\n\n"
        <<"    -l file\n"
        <<"           :Log all requests to the given file.\n\n"
        <<"    -p port\n"
        <<"           :Listen on the given port. If not provided, 8080 will be the default port.\n\n"
        <<"    -r dir\n"
        <<"           :Set the root directory for the http server to dir. \n\n"
        <<"    -t time\n"
        <<"           :Set the queuing time to time seconds. 60s is the default time\n\n"
        <<"    -n threadnum\n"
        <<"           :Set number of threads waiting ready in the execution thread pool to threadnum==4 by default.\n\n"
        <<"    -s sched\n"
        <<"           :Set the sheduling policy. It can be either CFS or SJF. The default will be FCFS.\n";
}

/**
	when cant find the index.html in the directory,
	then just generate fileindex html page for direcory
	subdirectory is append with '/'
	all the files and directories have their 'herf' attributes
**/
char* generateIndex(struct Request* r){
    string html="<html><p>Direcory:</p><table width=\"100%\" style=\"border:solid 1px #fff000;\"><tr><th>FILE</th><th>LAST MODIFIED TIME</th></tr>";
    struct dirent **file_name=NULL;
    int file_num;
    file_num=scandir(r->requestDir,&file_name,0,alphasort);
    if(file_num<0){
        html=html+"</table><p>empty directory!</p> </html>";
    }else{

        for(int i=0;i<file_num;i++){
			//skip hidden file
            if(!file_name[i]->d_name){
                html=html+"<p>empty directory!</p>";
                break;
            }
            if(file_name[i]->d_name[0] =='.'){
				continue;
			}


            struct stat buff;
            string subdir=string(r->requestDir)+string(file_name[i]->d_name);
            lstat(subdir.c_str(),&buff);
            if(S_ISDIR(buff.st_mode)){
					//if it is directory,add '/'
                html=html+"<tr><th><a href=\""+string(file_name[i]->d_name)+"/\">"+string(file_name[i]->d_name)+"</a></th>";
            }else{
					//else if it is a file
                html=html+"<tr><th><a href=\""+string(file_name[i]->d_name)+"\">"+string(file_name[i]->d_name)+"</a></th>";
            }
            html=html+"<th>"+string(lastmodifyTime(subdir.c_str()))+"</th></tr>";
        }
        html=html+"</table></html>";
    }
    //r->contentLength=html.size();//modify the content length
    char* buffer=(char*)malloc(html.size()*sizeof(char));
    strcpy(buffer,html.c_str());
    return buffer;
}



