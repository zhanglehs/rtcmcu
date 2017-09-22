/************************************************
*  duanbingnan, 2015-08-14
*  duanbingnan@youku.com
*  copyright:youku.com
*  ********************************************/

#include "dijkstra.h"

//Dijkstra algorithm to find out the shortest path
//param:
//        n : node number in the graph (total regions number)
//        v : source node (receiver's region)
//        dist[] : current node to source node's shortest path length
//        prev[] : for recording previous node of current node
//        c : record pingtime matrix of every two nodes in the graph

void Dijkstra(int n, int v, int *dist, int *prev, int c[maxnum][maxnum])
{
    bool s[maxnum];    // 判断是否已存入该点到S集合中
    for(int i=1; i<=n; ++i)
    {
        dist[i] = c[v][i];
        s[i] = 0;     // 初始都未用过该点
        if(dist[i] == maxint)
            prev[i] = 0;
        else
            prev[i] = v;
    }
    dist[v] = 0;
    s[v] = 1;

    // 依次将未放入S集合的结点中，取dist[]最小值的结点，放入结合S中
    // 一旦S包含了所有V中顶点，dist就记录了从源点到所有其他顶点之间的最短路径长度
    for(int i=2; i<=n; ++i)
    {
        int tmp = maxint;
        int u = v;
        // 找出当前未使用的点j的dist[j]最小值
        for(int j=1; j<=n; ++j)
            if((!s[j]) && dist[j]<tmp)
            {
                u = j;              // u保存当前邻接点中距离最小的点的号码
                tmp = dist[j];
            }
        s[u] = 1;    // 表示u点已存入S集合中

        // 更新dist
        for(int j=1; j<=n; ++j)
            if((!s[j]) && c[u][j]<maxint)
            {
                int newdist = dist[u] + c[u][j];
                if(newdist < dist[j])
                {
                    dist[j] = newdist;
                    prev[j] = u;
                }
            }
    }
}

// print the path
// param:
//          prev[]: for recording previous node of current node
//          v : source node (receiver's region)
//          u : end node (requestor's region)

void searchPath(int *prev,int v, int u)
{
    int que[maxnum];
    int tot = 1;
    que[tot] = u;
    tot++;
    int tmp = prev[u];
    while(tmp != v)
    {
        que[tot] = tmp;
        tot++;
        tmp = prev[tmp];
    }
    que[tot] = v;
    for(int i=tot; i>=1; --i)
        if(i != 1)
            std::cout << que[i] << "->";
        else
            std::cout << que[i] << std::endl;
}

