/*************************************************************************
	> File Name: main.cpp
	> Author: 朱紫钰
	> Mail: zhuziyu1157817544@gmail.com
	> Created Time: 2017年12月18日 星期一 18时39分45秒
 ************************************************************************/

#include<iostream>
#include"process.h"
using namespace std;
extern PROCESS process;
int main(int argc,char* argv[])
{
    return process.start(argc,argv);
    
}

