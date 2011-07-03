#include <iostream>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "apefs/apefilesystem.h"

using namespace std;

void makedir(const string& dir)
{
#ifdef WIN32
    mkdir(dir.c_str());
    //system(("mkdir \"" + dir + "\"").c_str());
#else
    mkdir(dir.c_str(), 777);
#endif
}

void printfs(ApeFileSystem& fs, const string &path = "/", int level = 0)
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
            printfs(fs, fs.joinpath(path, entries[i].name), level + 1);
        }
        else
        {
            cout << entries[i].name << endl;
        }
    }
}

bool backupfile(const string& filepath, const string& backuppath, ApeFileSystem& fs)
{
    fstream file;
    file.open(filepath.c_str(), ios::in | ios::out | ios::binary);
    if (!file.good())
        return false;

    ApeFile bfile(fs);
    if (!bfile.open(backuppath.c_str(), APEFILE_CREATE))
        return false;

    char buffer[1024];
    int count = 0;
    while (!file.eof())
    {
        file.read(buffer, sizeof(buffer));
        count += file.gcount();
        if (bfile.write(buffer, file.gcount()) != file.gcount())
            return false;
    }

    return true;
}

bool backup(const string& srcpath, ApeFileSystem& fs, const string& relpath = "/")
{
    dirent *entry;
    DIR *dp;

    dp = opendir(srcpath.c_str());
    if (dp == NULL)
        return false;

    while((entry = readdir(dp)))
    {
        struct stat s;
        stat(fs.joinpath(srcpath, entry->d_name).c_str(), &s);

        if (S_ISDIR(s.st_mode))
        {
            if (entry->d_name[0] == '.')
                continue;
            if (!fs.directorycreate(relpath + entry->d_name))
                return false;
            if (!backup(fs.joinpath(srcpath, entry->d_name), fs, fs.joinpath(relpath, entry->d_name)))
                return false;
        }
        else
        {
            if (!backupfile(fs.joinpath(srcpath, entry->d_name), fs.joinpath(relpath, entry->d_name), fs))
                return false;
        }
    }

    closedir(dp);

    return true;
}

bool restorefile(const string& backuppath, const string& filepath, ApeFileSystem& fs)
{
    fstream file;
    file.open(filepath.c_str(), ios::in | ios::out | ios::binary | ios::trunc);
    if (!file.good())
        return false;

    ApeFile bfile(fs);
    if (!bfile.open(backuppath, APEFILE_OPEN))
        return false;

    char buffer[1024];

    while (bfile.tell() < bfile.size())
    {
        int count = bfile.read(buffer, sizeof(buffer));
        file.write(buffer, count);
        if (!file.good())
            return false;
    }

    return true;
}

bool restore(const string& dstpath, ApeFileSystem& fs, const string& srcpath = "/")
{
    vector<ApeDirectoryEntry> entries;
    fs.directoryenum(srcpath, entries);
    for (int i = 0; i < entries.size(); i++)
    {
        if (entries[i].isdirectory())
        {
            makedir(fs.joinpath(dstpath, entries[i].name));
            if (!restore(fs.joinpath(dstpath, entries[i].name), fs, fs.joinpath(srcpath, entries[i].name)))
                return false;
        }
        else
        {
            if (!restorefile(fs.joinpath(srcpath, entries[i].name), fs.joinpath(dstpath, entries[i].name), fs))
                return false;
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    ApeFileSystem fs;
    string backupfs = "backup.apefs";
    string restorefolder;
    string backupfolder;

    if (argc > 3)
    {
        backupfolder = argv[1];
        restorefolder = argv[2];
    }
    else
    {
        char buffer[255];
        cout << "Backup folder:\n> ";
        cin.getline(buffer, 255);
        backupfolder = buffer;

        cout << "Restore folder:\n> ";
        cin.getline(buffer, 255);
        restorefolder = buffer;
    }

    cout << "Backup in progress..." << endl;

    if (!fs.create(backupfs, 1024 * 1024 * 100)) // 100 mb
    {
        cout << "Couldn't create filesystem\n";
        getchar();
        exit(1);
    }

    if (backup(backupfolder, fs))
    {
        cout << "Backup ok!" << endl << endl;
        cout << "Restoring..." << endl;
        fs.open(backupfs);
        makedir(restorefolder);
        if (restore(restorefolder, fs))
            cout << "Restore ok!" << endl << endl;
        else
            cout << "Restore error!" << endl << endl;
    }
    else
    {
        cout << "Backup error!" << endl << endl;
    }

    getchar();
    return 0;
}
