#include <iostream>
#include "apefs/apefilesystem.h"

using namespace std;

int main()
{
	
	ApeFileSystem fs;
	bool r;
	cout.flags(cout.flags() ^ ios_base::boolalpha);

	r = fs.create("lixo.apefs", 22222222);
	cout << r << endl;
	
	ApeInode inode;
	r = fs.directoryopen("/", inode);
	cout << r << " " << inode.num << endl;

	r = fs.directorycreate("/testdir1");
	cout << r << endl;

	r = fs.directorycreate("/testdir2");
	cout << r << endl;

	r = fs.directorycreate("/testdir2");
	cout <<  (r == false) << endl;

	r = fs.directorycreate("/testdir2/testdir2-1");
	cout <<  r << endl;

	r = fs.directorycreate("/testdir2/testdir2-1/testdir2-1-1");
	cout <<  r << endl;

	r = fs.directorycreate("/testdir2/testdir2-1/testdir2-1-1");
	cout <<  (r == false) << endl;

	r = fs.directoryopen("/testdir1", inode);
	cout << r << " " << inode.num << endl;

	r = fs.directoryopen("/testdir2", inode);
	cout << r << " " << inode.num << endl;

	r = fs.directoryopen("/testdir2/testdir2-1", inode);
	cout << r << " " << inode.num << endl;

	r = fs.directoryopen("/testdir2/testdir2-1/testdir2-1-1", inode);
	cout << r << " " << inode.num << endl;



	system("PAUSE");
    return 0;
}
