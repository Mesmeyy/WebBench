/*************************************************************************
	> File Name: process.h
	> Author: 朱紫钰
	> Mail: zhuziyu1157817544@gmail.com
	> Created Time: 2017年12月18日 星期一 17时12分47秒
 ************************************************************************/

#ifndef _PROCESS_H
#define _PROCESS_H

#include<unistd.h>
#include<stdio.h>
#include<sys/param.h>
#include<rpc/types.h>
#include<getopt.h>
#include<cstring>
#include<string>
#include<time.h>
#include<signal.h>
#include<algorithm>
#include<mutex>
#define METHOD_GET 0 //get请求的宏
#define METHOD_HEAD 1 //head请求的宏
#define PROGRAM_VERSION "zzy---1.5" //版本类型
#define MAXHOSTSIZE 256 //host最长256
#define MAXREQUESTSIZE 1024 //请求包最长1024
#define MAXPORT 65535//max port
#define MAXPTHREAD 100 //最多创建100线程
using namespace std;
using std::string;
using std::mutex;

class TTTHREAD
{
    /*本线程运行内部的相关信息*/
public:
    int speed;
    int fail;
    int bytes;
    TTTHREAD(int &s,int& f,int &b){
        speed = s;
        fail = f;
        bytes = b;
    }
};

class PROCESS
{
public:
    volatile int timerecpired;
    int allspeed;//可支持的访问数量
    int allfailed;//失败次数
    int allbytes;//传输比特
    int httpv;//http版本
    int method ;//请求方法
    int clients ;//线程个数
    int force;//是否强制返回
    int force_reload;//强制返回消息
    string proxyhost;//代理的host
    int proxyport;//代理的端口
    int benchtime;//倒计时
    string host;//host最大
    string request;//request最大
public:
    bool bench();//压力测试
    void benchcore(TTTHREAD& t);//压力测试核心
    void build_request(char*);//创建http请求包
    void usage(void);//使用说明函数
public:

    PROCESS();//构造函数
    int start(int argc,char*argv[]);//启动压力测试
};


/**本来想自己封装mutex，
* 但是看到了更好的轮子lock_guard*/
class LOCK
{
private:
    mutex m;
public:
    LOCK(PROCESS& process,TTTHREAD& th);
    
   
};
int mysocket(string ip,int port);
#endif
