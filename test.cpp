#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

using namespace std;

int main()
  {
  	string a = "--name=duong";
  	size_t pos = a.find("=");
  	string c = a.substr(2, pos -2);
  	cout << c<<endl;
  	string d = a.substr(pos+1);
  	cout << d;
  }