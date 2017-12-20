/*************************************************************************
	> File Name: process.cpp
	> Author: 朱紫钰
	> Mail: zhuziyu1157817544@gmail.com
	> Created Time: 2017年12月17日 星期日 21时46分30秒
 ************************************************************************/

#include<netdb.h>
#include<stdlib.h>
#include<stdarg.h>
#include<stdio.h>
#include<iostream>
#include<unistd.h>
#include<sys/param.h>
#include<rpc/types.h>
#include<getopt.h>
#include<cstring>
#include<pthread.h>
#include<vector>
#include<string>
#include<time.h>
#include<signal.h>
#include<condition_variable>
#include<sys/types.h>
#include<sys/stat.h>
#include<exception>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include"process.h"
#include<mutex>
#include<algorithm>
using namespace std;
using std::vector;
using std::string;
using std::mutex;
mutex mutex2;//用于clients变化时候的保护
mutex mutex3;//failed.
mutex mutex4;//speed
mutex mutex5;//用于bytes
void* sondo(void* p);//子线程函数
PROCESS process;
vector<pthread_t> tv;
static const struct option long_options[] = {
    {"force",no_argument,&process.force,1},
    {"reload",no_argument,&process.force_reload,1},
    {"time",required_argument,NULL,'t'},
    {"help",no_argument,NULL,'?'},
    {"http11",no_argument,NULL,'2'},
    {"get",no_argument,&process.method,METHOD_GET},
    {"head",no_argument,&process.method,METHOD_HEAD},
    {"version",no_argument,NULL,'V'},
    {"proxy",required_argument,NULL,'p'},
    {"clients",required_argument,NULL,'c'},
    {NULL,0,NULL,0}
};
static void signalhandle(int signal)
{
    process.timerecpired = 1;
}

LOCK::LOCK(PROCESS& process,TTTHREAD& th)
{
    lock_guard<mutex> m1(mutex3);
    process.allspeed += th.speed;
    lock_guard<mutex> m2(mutex4);
    process.allfailed += th.fail;
    lock_guard<mutex> m3(mutex5);
    process.allbytes += th.bytes;
}
PROCESS::PROCESS()
{
    timerecpired = 0;
    allspeed = 0;
    allfailed = 0;
    allbytes = 0;
    method = METHOD_GET;
    clients = 1;
    force = 0;
    force_reload = 0;
    proxyhost = "";
    proxyport = 80;
    benchtime = 30;
    host = "";
    request = "";
}
int PROCESS::start(int argc,char** argv)
{
    int opt=0;
    int options_index=0;
    int index;
    string  tempport;
    if(argc==1)
    {
        usage();
        return 2;
    } 
    
    while((opt=getopt_long(argc,argv,"2Vfrt:p:c:?h",long_options,&options_index))!=EOF )
    {
        switch(opt){
        case  0 : break;
        case 'f': force=1;break;
        case 'r': force_reload=1;break; 
        case '2': httpv=2;break;
        case 'V': cout << PROGRAM_VERSION << endl;exit(0);
        case 't': benchtime = atoi(optarg);break;	     
        
        case 'p': 
	        proxyhost = optarg;
            index = proxyhost.rfind(':',proxyhost.length() - 1);//找到最后一个:
            if(index == string::npos) return 2;
	        if(index == 0){
                cout << "108The hostname of the proxy can't be found" << endl;
                return 2;
	        }
	        if(index == proxyhost.length() - 1){
		        cout << "112Thi port of the proxy can't be found" << endl;
                return 2;
	        }
            proxyhost = proxyhost.substr(0,index);
	        tempport = proxyhost.substr(index+ 1,proxyhost.length()-index-1);
            proxyport = atoi(tempport.c_str());
            if(proxyhost.length()  > MAXHOSTSIZE) return 2;
            if(proxyport > MAXPORT) return 2;
            cout << "120 proxyhost:proxyport = " << proxyhost << ":" << proxyport<<endl; 
            break;
        case ':':
        case 'h':
        case '?': usage();return 2;break;
        case 'c': clients=atoi(optarg);break;
        }
    }
    
    if(optind==argc) {
	    cout << "130Can'tfound the texted url!"<<endl;
        usage();
	    return 2;
    }
    
    if(clients<=0) clients=1;
    if(clients > MAXPTHREAD) clients = MAXPTHREAD;
    if(benchtime<=0) benchtime=30;
    
    build_request(argv[optind]);
    
    cout << "Testing ..."<<endl;
    switch(method){
	case METHOD_GET:
	default:
		cout << "GET" << endl;break;
	case METHOD_HEAD:
		cout<<"HEAD"<<endl;break;
    }
    
    switch(httpv){
        default:httpv = 2;
    }
    
    cout << "Client = " << clients << endl;
    cout << "In the time is  " << benchtime << endl;
    if(force)cout<< "Early socket close"<<endl;
    if(proxyhost !="") cout << "The proxy server is "<< proxyhost << " : "<< proxyport<<endl;
    if(force_reload) cout << "Forcing reload" << endl;
    
    return bench();
    
}

void PROCESS::build_request(char *url)
{
    
    string surl = url;
    char tmp[10];
    int i;
    
    if(httpv != 2) httpv = 2;//只支持http1.1版本
    //zzy:构建http包的请求类型
    switch(method){
	  default:
	  case METHOD_GET: request += "GET";break;
	  case METHOD_HEAD:request += "HEAD";break;
    }
    
    request +=" ";
    
    
    //以下两个if语句监测url是否规范
    if(surl.length() > MAXHOSTSIZE) {
        cout << "181The checked url is too long." << endl;
        exit(2);
    }
    if(surl.find("://",0) == string::npos) {
        cout << "184The url is not standard which should be http://*."<<endl;
        exit(2);
    }
    
    if(proxyhost=="")
        //url头必须是http://开头
        if (0 != surl.compare(0,7,"http://")) {
            cout << "The only support protocol is http1.1"<<endl;
            exit(2);
        }
    
    i = surl.find("://",0) + 3;//http://www.baidu.com的第一个w的位置
    
    if(surl.find('/',i) <= i) {
        //url支持格式ex:
        //http://12.123.123.123/
        //http://12.123.123.123/1.*
        cout << "Invalid  url -hostname dont ends with '/'";
        exit(2);
    }
    
    if(proxyhost=="")
    {
        //if是:http://12.123.123.123:5656/的格式
        if((surl.find(':',i) != string::npos) && (surl.find(':',i) < surl.find('/',i)))
        {
	        host = host.assign(url+i,surl.find(':',i)-i);
	        //bzero(tmp,10);
	        //strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
            surl.assign(surl,surl.find(':',i)+1,surl.find('/',i)-i-1);
            proxyport=atoi(surl.assign(surl,surl.find(':',i)+1,surl.find('/',i)-i-1).c_str());
	        if(proxyport<=0) proxyport=80;
        }
        else
        {
            host = host.assign(url+i,surl.rfind('/',surl.length()-1) -i);
        }
        request += url+i+strcspn(url+i,"/");
    }
    else
    {
        request += url;
    }
    
	request +=" HTTP/1.1\r\n";
    
    if(httpv>0)  request += "User-Agent: WebBench "PROGRAM_VERSION"\r\n";
    
    if(proxyhost == "" )
    {
	    request += "Host: ";
	    request += host;
	    request +="\r\n";
    }
    
    if(force_reload && proxyhost !="")
    {
	    request += "Pragma: no-cache\r\n";
    }
    
    request += "Connection:close\r\n";
    request += "\r\n";  
    
}


bool PROCESS::bench(void)
{

    int tempspeed,tempfail,tempbytes;	
    FILE *f;
    int fd; 

    /*我觉得人家封的socket更好*/
    fd=mysocket(proxyhost==""?host:proxyhost,proxyport);
    if(fd < 0) { 
        cout << "Connection is fail" << endl;
        return 1;
    }
    close(fd);
    /*开始开线程了*/
    int i = 0;
    for(i=0;i<clients;i++)
    {
        pthread_t thid;
        pthread_create(&thid,NULL,sondo,NULL);
        tv.push_back(thid);
    }
	allspeed=0;
    allfailed=0;
    allbytes=0;
    
    for(auto &thr:tv)
    {
        pthread_join(thr,NULL);
    }
    cout << "线程都执行完毕了"<<endl;
    /*现在所有线程都执行完毕*/
    cout << "****** url ******" << endl;
    cout << host<<endl;
    cout << "speed = " << (allspeed+allfailed)/(benchtime/1.0) << "pages/sec"<<endl;
    cout << "suceed = "<< allspeed<<endl;
    cout << "fail = " << allfailed<<endl;
    cout << "byte/sec = " << allbytes/(benchtime/1.0)<<endl;
    return true;
}



//子线程的执行函数
void* sondo(void *p)
{
    FILE *fp;
    int speed = 0;
    int fail = 0;
    int bytes = 0;
    TTTHREAD temp(speed,fail,bytes);
    
    if(process.proxyhost == "")
        process.benchcore(temp);
    else
        process.benchcore(temp);
    try
    {
        LOCK alock(process,temp);
    }catch(exception& e){
        cout << e.what()<<endl;
    }
}

void PROCESS::benchcore(TTTHREAD& t)
{
    int rlen;
    char buf[1500];
    int fd,lenread;
    struct sigaction sa;

    sa.sa_handler = signalhandle;
    sa.sa_flags=0;
    if(sigaction(SIGALRM,&sa,NULL)) exit(3);
    alarm(benchtime);
    rlen=request.length();
    if(proxyhost !="") host = proxyhost;
nexttry:
    while(1)
    {
        //zzy:如果超时,就不给那个网站发数据了,也就是本分支的测试任务已经完成
        if(timerecpired) return;
        fd = mysocket(host,proxyport);                          
        
        //zzy:以下是各种发送出错情况
        if(fd<0) { 
            t.fail++;
            continue;
        }
        if(rlen!=write(fd,request.c_str(),rlen)) {
            t.fail++;
            close(fd);
            continue;
        }
        if(httpv != 2) 
	        if(shutdown(fd,1)) {
                t.fail++;
                close(fd);
                continue;
            }
        if(force==0) 
        {
	        while(1)
	        {
                if(timerecpired) break; 
	            lenread=read(fd,buf,1500);
                if(lenread < 0) 
                { 
                    t.fail++;
                    close(fd);
                    //zzy:读失败了无所谓,进行下一次压力发送即可
                    goto nexttry;
                }
	            else if(lenread==0) break;
                //zzy:这里的bytes代表本分支一共接收到的bytes数量
                else t.bytes+=lenread;
	        }
        }
        close(fd);
        t.speed++;
    }
}


int mysocket(string host,int port)
{
    int sock;
    unsigned long inaddr;
    struct sockaddr_in ad;
    struct hostent *hp;
    
    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;

    inaddr = inet_addr(host.c_str());
    if (inaddr != INADDR_NONE)
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));
    else
    {
        hp = gethostbyname(host.c_str());
        if (hp == NULL)
            return -1;
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
    }
    ad.sin_port = htons(port);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return sock;
    if (connect(sock, (struct sockaddr *)&ad, sizeof(ad)) < 0)
        return -1;
    return sock;
}


void PROCESS::usage(void)
{
   cout<< 
	"webbench [option]... URL\n"
	"  -f|--force               Don't wait for reply from server.\n"
	"  -r|--reload              Send reload request - Pragma: no-cache.\n"
	"  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
	"  -p|--proxy <server:port> Use proxy server for request.\n"
	"  -c|--clients <n>         Run <n> HTTP clients at once. Default 1.MAX 100\n"
	"  -2|--http11              Use HTTP/1.1 protocol.\n"
	"  --get                    Use GET request method.\n"
	"  --head                   Use HEAD request method.\n"
	"  -?|-h|--help             This information.\n"
	"  -V|--version             Display program version.\n"
	<<endl;
};

