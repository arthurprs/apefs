#ifndef APEFILESYSTEM_H
#define APEFILESYSTEM_H

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <pthread.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>

using namespace std;

const uint32_t BlockSIZE = 1024*4; // 4kb
const uint32_t MAXBlockS = 0xffff;
const uint32_t MAXFILESIZE = BlockSIZE * MAXBlockS;
const uint32_t MAXFILES = 0xffffffff;

typedef uint32_t inodeid_t;
typedef uint16_t blocknum_t;


struct ApeBlock
{
    blocknum_t num;
    uint8_t data[BlockSIZE];
};

/*
    The main file header.
    Sits at the end of the file
 */
struct ApeFileSystemHeader
{
    char magic[5]; // "apefs"
    uint8_t version;
    uint32_t inodestablesize; // inodes table size in bytes
    uint32_t freeBlockstablesize; // freeBlocks table size in bytes
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
    inodeid_t id; // "inode number"
    inodeid_t parentid; // parent inode
    uint8_t flags;
    clock_t creationtime;
    clock_t lastmodificationtime;
    clock_t lastaccesstime;
    uint32_t size; // size in bytes
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
    vector<blocknum_t> Blocks;
    uint16_t references; // number of objects pointing to this inode
    inline bool isFolder()
    {
        return flags && APEFLAG_FOLDER;
    };
    inline bool isFile()
    {
        return flags && APEFLAG_FILE;
    };
    inline uint16_t unref()
    {
        return --references;
    };
    inline uint16_t ref()
    {
        return ++references;
    };

};

/*
    Directory entry in disk
*/
struct ApeDirectoryEntryRaw
{
    inodeid_t inodeid;
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
    ApeFile(const ApeInode& inode, const ApeFileSystem& owner);
    ~ApeFile();
    uint32_t read(void* buffer, uint32_t size);
    uint32_t write(void* buffer, uint32_t size);
    uint32_t tell();
    uint32_t seek(ApeFileSeekMode mode, int32_t offset);
    uint32_t size();
    bool close();

private:
    uint32_t position;
    const ApeInode& inode_;
    const ApeFileMode mode_;
    const ApeFileSystem& owner_;
};

class ApeDirectory
{
public:
    ApeDirectory(const ApeInode& inode, const ApeFileSystem& owner);
    bool listfiles(vector<string>& files);
    bool listdirectories(vector<string>& directories);

private:
    map<string, inodeid_t> entriescache_; // cache for fast lookup
    const ApeInode& inode_;
    const ApeFileSystem& owner_;
};


class ApeFileSystem
{
public:
    ApeFileSystem(const string& fspath);
    ~ApeFileSystem();

    // maintence related
    void defrag(); // defragment files (also compact free Blocks)
    // block related
    bool blockfree(blocknum_t blocknum);
    bool blockread(blocknum_t blocknum, ApeBlock& block);
    bool blockwrite(const ApeBlock& block);
    bool blockalloc(const ApeInode& inode, ApeBlock& block);
    // inode related
    bool inodefree(inodeid_t inodeid);
    bool inoderead(inodeid_t inodeid, ApeInode& inode);
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

private :
    fstream file_;
    map<blocknum_t, ApeBlock> cachedblocks_;
    map<inodeid_t, ApeDirectory> cachedfolders_; // map of cached folders
    map<inodeid_t, ApeInode> cachedinodes_; // map of cached inodes
    map<inodeid_t, uint32_t> inodes_; // map of inodes on disk
    vector<blocknum_t> freeblocks_; // list of free Blocks
};

#endif // APEFILESYSTEM_H
