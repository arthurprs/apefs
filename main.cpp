#include <iostream>
#include "apefs/apefilesystem.h"

using namespace std;

int main()
{
	ApeFileSystem fs;
	fs.create("lixo.apefs", 22222222);
	system("PAUSE");
    return 0;
}
