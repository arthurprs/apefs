#include <iostream>
#include "apefs/apefilesystem.h"

using namespace std;

void print(ApeFileSystem& fs, const string &path, int level = 0)
{
	vector<ApeDirectoryEntry> entries;
	fs.directoryenum(path, entries);
	for (int i = 0; i < entries.size(); i++)
	{
		for (int l = 0; l < level; l++)
		{
			cout << " ";
		}
		if (entries[i].isdirectory())
		{
			cout << "+ " << entries[i].name << endl;
			print(fs, fs.joinpath(path, entries[i].name), level + 1);
		}
		else
		{
			cout << entries[i].name << endl;
		}
	}
}

void test()
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

	print(fs, "/");

	r = fs.directorydelete("/testdir2/testdir2-1/testdir2-1-1");
    cout << r << endl;

	r = fs.directorycreate("/testdir2/testdir2-1/testdir2-1-1");
	cout <<  r << endl;

    r = fs.directorydelete("/testdir1");
    cout << r << endl;

	r = fs.directorycreate("/testdir1");
	cout << r << endl;

	r = fs.directorycreate("/testdir2");
	cout << (r == false) << endl;

	r = fs.directorycreate("/testdir3");
	cout << r << endl;

	r = fs.directorydelete("/testdir2");
	cout << (r == false) << endl;

	r = fs.directorydelete("/testdir3");
	cout << r << endl;

	r = fs.directorydelete("/testdir4");
	cout << (r == false) << endl;

	r = fs.directorycreate("/testdir4");
	cout << r << endl;

	r = fs.directorydelete("/testdir4");
	cout << r << endl;

	ApeFile file(fs);

	r = file.open("/testdir1/myfile.txt", APEFILE_CREATE);
	cout << r << endl;

	r = fs.filedelete("/testdir1/myfile.txt");
	cout << r << endl;

	r = file.open("/testdir1/myfile.txt", APEFILE_OPEN);
	cout << (r == false) << endl;
	
	r = file.open("/testdir1/myfile.txt", APEFILE_CREATE);
	cout << r << endl;

	r = file.open("/testdir1/myfile.txt", APEFILE_OPEN);
	cout << r << endl;

	string writestrbuffer = "my file content :D\n";
	for (int i = 0; i < 10; i++)
	{
		r = writestrbuffer.length() == file.write(writestrbuffer.c_str(), writestrbuffer.length());
		cout << r << endl;
	}

	char* writebuffer = new char[BLOCKSIZE + 10];
	memset(writebuffer, 'a', BLOCKSIZE + 10);
	char* readbuffer = new char[BLOCKSIZE + 10];

	r = BLOCKSIZE + 10 == file.write(writebuffer, BLOCKSIZE + 10);
	cout << r << endl;

	r = file.seek(APESEEK_CUR, -(BLOCKSIZE + 10));
	cout << r << endl;

	r = BLOCKSIZE + 10 == file.read(readbuffer, BLOCKSIZE + 10);
	cout << r << endl;

	cout << (memcmp(writebuffer, readbuffer, BLOCKSIZE + 10) == 0) << endl;

	delete[] writebuffer;
	delete[] readbuffer;

	r = file.seek(APESEEK_SET, 0);
	cout << r << endl;

	string readstrbuffer;
	readstrbuffer = writestrbuffer;
	for (int i = 0; i < 10; i++)
	{
		r = writestrbuffer.length() == file.read((void*)readstrbuffer.data(), writestrbuffer.length());
		cout << r << endl;
		cout << (readstrbuffer == writestrbuffer) << endl;
	}

	print(fs, "/");
}

int main()
{
    test();
   	system("PAUSE");
    return 0;
}
