#include <iostream>
#include "apefs/apefilesystem.h"

using namespace std;

int main()
{
	ApeFileSystem fs;
	bool r;
	r = fs.create("lixo.apefs", 22222222);
	cout << r << endl;
	
	
	
	system("PAUSE");
    return 0;
}
