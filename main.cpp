#include <iostream>
#include "apefs/apefilesystem.h"

using namespace std;

int main()
{
	ApeFileSystem fs;
	bool r;
	r = fs.create("lixo.apefs", 22222222);
	cout << r << endl;
	
	ApeInode inode;
	r = fs.inodealloc(inode);
	cout << "inodealloc() " << r << endl;
	ApeBlock block;
	r = fs.blockalloc(inode, block);
	cout << "blockalloc() " << r << endl;
	
	system("PAUSE");
    return 0;
}
