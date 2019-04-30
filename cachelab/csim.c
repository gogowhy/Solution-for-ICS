/*#include "cachelab.h"
 
 int main()
 {
 printSummary(0, 0, 0);
 return 0;
 }*/

/*id=517021910679
 
 */

//test
#include "cachelab.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#define INF 0x2fffffff

long unsigned int Hits=0,Misses=0,Evictions=0;
// cache的结构与三位数组相似，因此考虑使用三位数组  第一维表示组数，第二维表示每组的行数，第三维就是每行的三个值有效位，标志位以及LRU值。
long unsigned int caches,cacheE,cacheb; //cache的组，行，块
long unsigned int ***cache;

void set_the_cache(long unsigned int s,long unsigned int E,long unsigned int b){   //初始化cache的s，E，b函数
    caches=s;cacheE=E,cacheb=b;
}

long unsigned int get_the_valid(long unsigned int s,long unsigned int E){return cache[s][E][0];}    //返回第s组第E行的有效位
long unsigned int Get_Tag(long unsigned int s,  long unsigned int E){return cache[s][E][1];}    //返回第s组地E行的tag
long unsigned int Get_Lru(long unsigned int s,  long unsigned int E){return cache[s][E][2];}    //返回第s组第E行的LRU记数值



void get_the_cache(){         //动态为这个数组分配内存
    cache=(long unsigned int ***)malloc(caches*sizeof(long unsigned int **));
    for(int i=0;i<caches;i++)
    {
        cache[i]=(long unsigned int **)malloc(cacheE*sizeof(long unsigned int *));
    }
    for(int i=0;i<caches;i++)
    {
        for(int j=0;j<cacheE;j++)
        {
            cache[i][j]=(long unsigned int *)malloc(3*sizeof(long unsigned int));
        }
    }
    for(int i=0;i<caches;i++)     //初始化cache内容
    {
        for(int j=0;j<cacheE;j++)
        {
            cache[i][j][0]=0;
            cache[i][j][1]=0;
            cache[i][j][2]=INF;
        }
    }
}

long unsigned int get_the_LRU(long unsigned int s){      //查询LRU最小的行号
    long unsigned int Lru=INF,Pos=0;
    for(int i=0;i<cacheE;i++)
    {
        if(get_the_valid(s,i)==0)
        {
            return i;
        }
        if(Lru>Get_Lru(s,i))
        {
            Lru=Get_Lru(s,i);Pos=i;
        }
    }
    return Pos;
}

int Is_Hit(long unsigned int s,long unsigned int Tag){    //判断是否命中，命中了返回行号，否则返回1
    for(int i=0;i<cacheE;i++)
    {
        if(cache[s][i][0]==1&&cache[s][i][1]==Tag)
        {
            return i;
        }
    }
    return -1;
}

void Replace(long unsigned int s,long unsigned int E,long unsigned int Tag){   //替换第_s组中的第_E行，用于替换的标志位为Tag；
    if(get_the_valid(s,E)==1&&Get_Tag(s,E)!=Tag)
    {
        ++Evictions;
    }
    cache[s][E][0]=1;
    cache[s][E][1]=Tag;
    cache[s][E][2]=INF;
    for(int i=0;i<cacheE;i++)
    {
        if(i!=E)
        {
            --cache[s][i][2];
        }
    }
}

void search_solve(long unsigned int S,long unsigned int Tag){
    int IS_HIT=Is_Hit(S,Tag);         //判断是否命中，命中则返回命中第S组中的行数，否则返回-1；
    if(IS_HIT==-1){                  //如果没有命中，先++miss，然后按照LRU替换策略进行替换；
        ++Misses;
        Replace(S,get_the_LRU(S),Tag);
    }
    else {                           //如果命中，则替换命中的行，其实这里没有必要替换，主要是要更新LRU值。
        ++Hits;
        Replace(S,IS_HIT,Tag);
    }
}


int main(int argc,char *const argv[]){
	
	FILE *Path=NULL;
	char opt,cmd[2],Ch;
	int s=-1,E=-1,b=-1;
	while((opt=getopt(argc,argv,"hvs:E:b:t:"))!=-1){
		switch(opt){
			case 's':
				s=atol(optarg);break;     //atol字符串转化为整数
			case 'E':
				E=atol(optarg);break;
			case 'b':
				b=atol(optarg);break;
			case 't':
				Path=fopen(optarg,"r");break;
		
		}
	}
    set_the_cache(1<<(long unsigned int)s,E,1<<(long unsigned int)b);
	get_the_cache();
	long unsigned int addr,number,Tag,S;
	while(fscanf(Path,"%s%lx%c%lu",cmd,&addr,&Ch,&number)!=EOF){
		if(cmd[0]=='I')continue;
		Tag=addr>>(s+b);
		S=(addr&((1lu<<(s+b))-1))>>b;
		if(cmd[0]=='L'||cmd[0]=='S')
			search_solve(S,Tag);
		else {
			search_solve(S,Tag);
			search_solve(S,Tag);
		}
	}
	printSummary(Hits,Misses,Evictions);
    return 0;
}


