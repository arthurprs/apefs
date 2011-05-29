/*

*/

#ifndef APEFILESYSTEM_H
#define APEFILESYSTEM_H

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdint.h>
#include "apebitmap.h"

using namespace std;

const uint32_t BLOCKSIZE = 1024*4; // 4kb
const uint32_t MAXINODES = 3 * BLOCKSIZE * 8; // 3 map blocks ~~ 100k inodes

typedef uint32_t inodenum_t;
typedef uint32_t blocknum_t;

const inodenum_t INVALIDINODE = (-1);
const blocknum_t INVALIDBLOCK = (-1);

struct ApeBlock
{
    blocknum_t num;
    uint8_t data[BLOCKSIZE];
};

/*
    The main file header.
    Sits at the end of the file
 */
struct ApeSuperBlock
{
    char magic[5]; // "apefs"
    uint8_t version;
    uint32_t filesystemsize;
    uint32_t blockmaps; // number of block maps after the superblock
    uint8_t inodemaps; // number of inode maps after block maps
    uint8_t inodeblocks; // number of blocks reserved for inode table
};

/*
    Inode flags for extra info
*/
const uint8_t APEFLAG_FILE = 1;
const uint8_t APEFLAG_DIRECTORY = 2;

/*
    mimics a real unix inode
    the raw part is saved/loaded from the file directly
*/
struct ApeInodeRaw
{
    inodenum_t num; // "inode number"
    uint8_t flags;
    uint32_t size; // size in bytes
    uint16_t blockscount;
    /*
        8 direct blocks -> 8 * 4096 = 32kb
        1 indirect block table -> 1024 * 4096 = 4mb
        1 double indirect block table -> 1024 * 1024 * 4096 = 4gb
    */
    blocknum_t blocks[10];
};

const uint32_t INODESPERBLOCK = BLOCKSIZE / sizeof(ApeInodeRaw);

/*
    The above raw inode structure
    plus variable size info, like data Blocks numbers
*/
struct ApeInode: ApeInodeRaw
{
    bool isdirectory() const;
    bool isfile() const;
};

/*
    Directory entry in disk
*/
struct ApeDirectoryEntryRaw
{
    inodenum_t inodenum;
    uint8_t flags;
    uint16_t entrysize;
    uint8_t namelen;
    uint16_t realsize() const;
    uint16_t freesize() const;
    bool isdirectory() const;
    bool isfile() const;
};

struct ApeDirectoryEntry : ApeDirectoryEntryRaw
{
    string name;
    void read(void *buffer);
    void write(void *buffer);
};

/*
    forward class declarations..
*/
class ApeDirectory;
class ApeFile;
class ApeFileSystem;

/*
    File open mode
*/
enum ApeFileMode {APEFILE_CLOSE, APEFILE_OPEN, APEFILE_CREATE, APEFILE_APPEND};

/*
    File seek mode
*/
enum ApeFileSeekMode {APESEEK_SET, APESEEK_CUR, APESEEK_END};

/*
    Represents a file in the filesystem
*/
class ApeFile
{
public:
    ApeFile(ApeFileMode mode, ApeInode& inode, ApeFileSystem& owner);
    ~ApeFile();
    uint32_t read(void* buffer, uint32_t size);
    uint32_t write(void* buffer, uint32_t size);
    bool seek(ApeFileSeekMode mode, int32_t offset);
    uint32_t tell() const;
    uint32_t size() const;
    bool close();
private:
    uint32_t position_;
    ApeInode& inode_;
    ApeFileSystem& owner_;
    ApeFileMode mode_;
};

class ApeFileSystem
{
public:
    ApeFileSystem();
    ~ApeFileSystem();
    // block related
    bool blockfree(blocknum_t blocknum);
    bool blockread(blocknum_t blocknum, ApeBlock& block);
    bool blockread(const ApeInode& inode, uint32_t blockpos, ApeBlock& block);
    bool blockwrite(ApeBlock& block);
	bool blockalloc(ApeBlock& block, uint8_t fillbyte = 0);
    bool blockalloc(ApeInode& inode, ApeBlock& block);
    // inode related
    bool inodefree(inodenum_t inodenum);
    bool inoderead(inodenum_t inodenum, ApeInode& inode);
    bool inodewrite(ApeInode& inode);
    bool inodealloc(ApeInode& inode);
    // file related
    bool fileexists(const string& filepath);
    bool fileopen(const string& filepath, ApeFileMode mode, ApeFile& file);
    bool filedelete(const string& filepath);
    // directory related
    bool directoryaddentry(ApeInode& inode, ApeDirectoryEntry& entry);
    bool directoryremoveentry(ApeInode& inode, const string& name);
    bool directoryfindentry(ApeInode& inode, const string& name, ApeDirectoryEntry& entry);
    bool directoryexists(const string& path);
    bool directorycreate(const string& path);
    bool directoryopen(const string& path, ApeInode& inode);
    bool directorydelete(const string& path);

    bool open(const string &fspath);
    bool create(const string& fspath, uint32_t fssize);
    bool close();
    uint32_t size() const;

private:
    uint32_t inodesoffset_;
    uint32_t blocksoffset_;
    ApeSuperBlock superblock_;
    fstream file_;
    ApeBitMap inodesbitmap_;
    ApeBitMap blocksbitmap_;
	static bool parsepath(const string &path, vector<string> &parsedpath);
	static string extractdirectory(const string &path);
	static string extractfilename(const string& path);
};

#endif // APEFILESYSTEM_H
