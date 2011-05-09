/*

*/

#ifndef APEFILESYSTEM_H
#define APEFILESYSTEM_H

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <pthread.h>
#include <stdint.h>

using namespace std;

const uint32_t BLOCKSIZE = 1024*4; // 4kb

typedef uint32_t inodenum_t;
typedef uint32_t blocknum_t;

const inodenum_t INVALIDINODE = (-1);
const blocknum_t INVALIDBLOCK = (-1);

struct ApeBlock
{
    blocknum_t num;
    uint8_t data[BLOCKSIZE];
    bool dirty;
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
    uint8_t inodemaps; // number of inode maps after the superblock
    uint8_t blockmaps; // number of block maps after the inode maps
};

/*
    Inode flags for extra info
*/
const uint8_t APEFLAG_FILE = 1;
const uint8_t APEFLAG_FOLDER = 2;
const uint8_t APEFLAG_DUMMY = 4;

/*
    mimics a real unix inode
    the raw part is saved/loaded from the file directly
*/
struct ApeInodeRaw
{
    inodenum_t num; // "inode number"
    inodenum_t parentid; // parent inode
    uint8_t flags;
    clock_t creationtime;
    clock_t lastmodificationtime;
    clock_t lastaccesstime;
    uint32_t size; // size in bytes
    uint16_t blockscount;
    /*
        8 direct blocks -> 8 * 4096 = 32kb
        1 indirect block table -> 1024 * 4096 = 4mb
        1 double indirect block table -> 1024 * 1024 * 4096 = 4gb
    */
    blocknum_t blocks[10];
};

/*
    The above raw inode structure
    plus variable size info, like data Blocks numbers
*/
struct ApeInode: ApeInodeRaw
{
    bool dirty;
    uint16_t references; // number of objects pointing to this inode
    bool isfolder() const;
    bool isfile() const;
    uint16_t unref();
    uint16_t ref();
};

/*
    Directory entry in disk
*/
struct ApeDirectoryEntryRaw
{
    inodenum_t inodeid;
    uint8_t flags;
    uint8_t entrysize;
    uint8_t namelen;
};

struct ApeDirectoryEntry : ApeDirectoryEntryRaw
{
    string name;
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

class ApeDirectory
{
public:
    ApeDirectory(ApeInode& inode,  ApeFileSystem& owner);
    bool addentry(const string& name, inodenum_t inodenum);
    bool removeentry(const string& name);
    bool findentry(const string& name, inodenum_t& inodenum);
    bool isempty();

private:
    map<string, inodenum_t> entriescache_; // cache for fast entry lookup
    ApeInode& inode_;
    ApeFileSystem& owner_;
};


class ApeFileSystem
{
public:
    ApeFileSystem();
    ~ApeFileSystem();
    // maintence related
    void defrag(); // defragment files (also compact free Blocks)
    // block related
    bool blockfree(blocknum_t blocknum);
    bool blockread(blocknum_t blocknum, ApeBlock& block);
    bool blockread(const ApeInode& inode, uint32_t blocknum, ApeBlock& block);
    bool blockwrite(const ApeBlock& block);
    bool blockalloc(ApeInode& inode, ApeBlock& block);
    // inode related
    bool inodefree(inodenum_t inodenum);
    bool inoderead(inodenum_t inodenum, ApeInode& inode);
    bool inodewrite(const ApeInode& inode);
    bool inodealloc(ApeInode& inode);
    // file related
    bool fileexists(const string& filepath);
    bool fileopen(const string& filepath, ApeFileMode mode, ApeFile& file);
    bool filedelete(const string& filepath);
    // directory related
    bool directoryexists(const string& filepath);
    bool directoryopen(const string& directorypath, ApeDirectory& directory);
    bool directorydelete(const string& filepath);

    bool open(const string& fspath);
    bool close();
private :
	uint32_t blocksoffset;
    fstream file_;
    map<blocknum_t, ApeBlock> cachedblocks_;
    map<inodenum_t, ApeDirectory> cachedfolders_; // map of cached folders
    map<inodenum_t, ApeInode> cachedinodes_; // map of cached inodes
};

#endif // APEFILESYSTEM_H
