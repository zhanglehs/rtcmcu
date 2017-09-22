/************************************************
*  duanbingnan, 2015-08-14
*  duanbingnan@youku.com
*  copyright:youku.com
*  ********************************************/

#ifndef _DIJKSTRA_H_
#define _DIJKSTRA_H_

#include <iostream>
#include <stdio.h>

static const int maxnum = 100;
static const int maxint = 999999;

void Dijkstra(int n, int v, int *dist, int *prev, int c[maxnum][maxnum]);
void searchPath(int *prev,int v, int u);

#endif /* _DIJKSTRA_H_ */
