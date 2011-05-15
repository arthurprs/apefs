#include "apefilesystem.h"

#include <math.h>

inline bool ApeInode::isdirectory() const
{
    return (flags & APEFLAG_DIRECTORY) != 0;
}

inline bool ApeInode::isfile() const
{
    return (flags & APEFLAG_FILE) != 0;
}

ApeFileSystem::ApeFileSystem()
{

}

ApeFileSystem::~ApeFileSystem()
{
    close();
}

bool ApeFileSystem::open(const string &fspath)
{
    file_.open(fspath.c_str(), ios::in | ios::out | ios::binary | ios::app);
    if (!file_.good())
        return false;

    // read superblock
    file_.seekp(0);
    file_.read((char*)&superblock_, sizeof(ApeSuperBlock));

    // verify if it's valid
    if (strncmp("apefs", superblock_.magic, 5) != 0)
        return false;

    // set the offsets
    inodesoffset_ = sizeof(ApeSuperBlock) + (superblock_.inodemaps + superblock_.blockmaps) * BLOCKSIZE;
    blocksoffset_ = inodesoffset_ + superblock_.inodeblocks * BLOCKSIZE;

    // read bitmaps into memory
    int buffersize = max(superblock_.blockmaps, (uint32_t)superblock_.inodemaps) * BLOCKSIZE;
    char *buffer = new char[buffersize];

    buffersize = superblock_.blockmaps * BLOCKSIZE;
    file_.read(buffer, buffersize);
    blocksbitmap_.frombuffer(buffer, buffersize);

    buffersize = superblock_.inodemaps * BLOCKSIZE;
    file_.read(buffer, buffersize);
    inodesbitmap_.frombuffer(buffer, buffersize);

    delete[] buffer;

    return file_.good();
}

bool ApeFileSystem::close()
{
    file_.close();
    return file_.good();
}

ApeFile::ApeFile(ApeFileMode mode, ApeInode& inode, ApeFileSystem& owner)
    : inode_(inode), owner_(owner), mode_(mode)
{
    switch (mode_)
    {
    case APEFILE_APPEND:
        position_ = inode_.size;
        break;

    case APEFILE_CREATE:
        position_ = 0;
        break;

    case APEFILE_OPEN:
        position_ = 0;
        break;

    default:
        break;
    }
}

bool ApeFile::close()
{
    if (mode_ == APEFILE_CLOSE)
        return false;

    return true;
}

uint32_t ApeFile::read(void* buffer, uint32_t count)
{
    ApeBlock block;
    uint32_t bytesread;

    if (mode_ == APEFILE_CLOSE)
        return 0;

    bytesread = 0;
    while (bytesread < count && position_ < inode_.size)
    {
        uint32_t bytestoread = min(count % BLOCKSIZE, inode_.size - position_);
        if (!owner_.blockread(position_ / BLOCKSIZE, block))
            return 0;
        memcpy(&((uint8_t*)buffer)[bytesread], block.data, bytestoread);
        position_ += bytestoread;
        bytesread += bytestoread;
    }
    return bytesread;
}

bool ApeFile::seek(ApeFileSeekMode seekmode, int32_t offset)
{
    if (mode_ == APEFILE_CLOSE)
        return false;

    switch (seekmode)
    {
    case APESEEK_CUR:
        if (offset >= 0 && position_ + offset < inode_.size)
        {
            position_ += offset;
            return true;
        }
        break;

    case APESEEK_END:
        if (offset <= 0 && inode_.size + offset >= 0)
        {
            position_ = inode_.size + offset;
            return true;
        }
        break;

    case APESEEK_SET:
        if (offset >= 0 && (uint32_t)offset < inode_.size)
        {
            position_ = offset;
            return true;
        }
        break;
    }

    return false;
}

inline uint32_t ApeFile::size() const
{
    return inode_.size;
}

inline uint32_t ApeFile::tell() const
{
    return position_;
}

uint32_t ApeFile::write(void* buffer, uint32_t size)
{
    return 0;
}

ApeFile::~ApeFile()
{
    close();
}

bool ApeFileSystem::blockalloc(ApeInode& inode, ApeBlock& block)
{
    uint32_t freebit = blocksbitmap_.findunsetbit();
    if (freebit == NOBIT)
        return false;
    blocksbitmap_.setbit(freebit);
    memset(&block, 0, sizeof(ApeBlock));
    block.num = freebit;
    return true;
}

bool ApeFileSystem::blockfree(blocknum_t blocknum)
{
    return blocksbitmap_.unsetbit(blocknum);
}

bool ApeFileSystem::blockread(blocknum_t blocknum, ApeBlock& block)
{
    file_.seekp(blocksoffset_ + blocknum * BLOCKSIZE);
    block.num = blocknum;
    file_.read((char*)&block.data, BLOCKSIZE);
    return file_.good();
}

bool ApeFileSystem::blockread(const ApeInode& inode, uint32_t blocknum, ApeBlock& block)
{
    if (blocknum > inode.blockscount)
        return false;
    if (blocknum < 8)
        return blockread(inode.blocks[blocknum], block);

    uint32_t rnum = blocknum - 8;
    ApeBlock iblock;

    if (!blockread(inode.blocks[9], iblock))
        return false;
    if (rnum < 1024)
    {
        return blockread(iblock.data[rnum], block);
    }
    else
    {
        ApeBlock diblock;
        if (!blockread(iblock.data[rnum / 1024], diblock))
            return false;
        return blockread(diblock.data[rnum % 1024], block);
    }
}

bool ApeFileSystem::blockwrite(ApeBlock& block)
{
    file_.seekp(blocksoffset_ + block.num * BLOCKSIZE);
    file_.write((char*)&block.data, BLOCKSIZE);
    return file_.good();
}

bool ApeFileSystem::directorydelete(const string& path)
{
    return false;
}

bool ApeFileSystem::directoryexists(const string& path)
{
    return false;
}

bool ApeFileSystem::directorycreate(const string& path)
{
    return false;
}

bool ApeFileSystem::directoryopen(const string& path, ApeInode& inode)
{
    vector<string> parsedpath;
    
    ApeDirectoryEntry tempentry;
    ApeInode tempinode;
    // open root inode
    if (!inoderead(0, tempinode))
        return false;

    if (path == "/")
        return true;

    if (!parsepath(path, parsedpath))
        return false;

    // go though all folders
    for (size_t i = 0; i < parsedpath.size(); i++)
    {
        if (!directoryfindentry(tempinode, parsedpath[i], tempentry))
            return false;
        if (!tempentry.isdirectory())
            return false;
        if (!inoderead(tempentry.inodenum, tempinode))
            return false;
    }

    inode = tempinode;
    return true;
}

bool ApeFileSystem::filedelete(const string& filepath)
{
    return false;
}

bool ApeFileSystem::fileexists(const string& filepath)
{
    return false;
}

bool ApeFileSystem::fileopen(const string& filepath, ApeFileMode mode, ApeFile& file)
{



    return false;
}

bool ApeFileSystem::inodealloc(ApeInode& inode)
{
    uint32_t freebit = inodesbitmap_.findunsetbit();
    if (freebit == NOBIT)
        return false;
    inodesbitmap_.setbit(freebit);
    memset(&inode, 0, sizeof(ApeInode));
    inode.num = freebit;
    return true;
}

bool ApeFileSystem::inodefree(inodenum_t inodenum)
{
    return inodesbitmap_.unsetbit(inodenum);
}

bool ApeFileSystem::inoderead(inodenum_t inodenum, ApeInode& inode)
{
    file_.seekp(inodesoffset_ + inodenum * sizeof(ApeInodeRaw));
    file_.read((char*)&inode, sizeof(ApeInodeRaw));
    return file_.good();
}

bool ApeFileSystem::inodewrite(ApeInode& inode)
{
    file_.seekp(inodesoffset_ + inode.num * sizeof(ApeInodeRaw));
    file_.write((char*)&inode, sizeof(ApeInodeRaw));
    return file_.good();
}

bool ApeFileSystem::create(const string& fspath, uint32_t fssize)
{
    file_.open(fspath.c_str(), ios::in | ios::out | ios::binary | ios::trunc);
    if (!file_.good())
        return false;

    // set the superblock
    superblock_.blockmaps = (uint8_t) ceil((float)fssize / BLOCKSIZE);
    superblock_.filesystemsize = superblock_.blockmaps * BLOCKSIZE;
    superblock_.inodemaps = MAXINODES / BLOCKSIZE / 8;
    superblock_.inodeblocks = (uint8_t) ceil((float)BLOCKSIZE + sizeof(ApeInodeRaw));
    strcpy(superblock_.magic, "apefs");
    superblock_.version = 1;

    // set the offsets
    inodesoffset_ = sizeof(ApeSuperBlock) + (superblock_.inodemaps + superblock_.blockmaps) * BLOCKSIZE;
    blocksoffset_ = inodesoffset_ + superblock_.inodeblocks * BLOCKSIZE;

    // write superblock to file
    file_.write((char*)&superblock_, sizeof(ApeSuperBlock));

    // write all necessary blanks
    char *blank = new char[BLOCKSIZE];
    memset(blank, 0, BLOCKSIZE);

    int totalblanks = superblock_.blockmaps + superblock_.inodemaps + superblock_.inodeblocks;
    for (int i = 0; i < totalblanks; i++)
        file_.write(blank, BLOCKSIZE);

    delete[] blank;

    // init bitmaps
    blocksbitmap_.reserve(superblock_.blockmaps * BLOCKSIZE);
    blocksbitmap_.unsetall();
    inodesbitmap_.reserve(superblock_.inodemaps * BLOCKSIZE);
    inodesbitmap_.unsetall();

    // create root folder
    ApeInode rootinode;
    inodealloc(rootinode);
    rootinode.flags = APEFLAG_DIRECTORY;
    inodewrite(rootinode);

    return true;
}


inline uint32_t ApeFileSystem::size() const
{
    return superblock_.filesystemsize;
}

bool ApeFileSystem::parsepath(const string &path, vector<string> &parsedpath)
{
    parsedpath.clear();
    int sep = path.find('/');
    while (sep != string::npos)
    {
        int nextsep = path.find('/', sep + 1);
        string piece = path.substr(sep + 1, nextsep - 1 - sep);
        if (piece.length() > 0)
            parsedpath.push_back(piece);
        else if (sep != path.length() - 1)
            return false;
        sep = nextsep;
    }
    return parsedpath.empty();
}

bool ApeFileSystem::directoryaddentry(ApeInode& inode, ApeDirectoryEntry& entry)
{
    ApeBlock block;
    entry.namelen = entry.name.length();
    entry.entrysize = entry.realsize();
    
    int n = -1;
    while (blockread(inode, ++n, block))
    {
        ApeDirectoryEntryRaw *ientry;
        int entryfreesize;
        int i = 0;

        do
        {
            ientry = (ApeDirectoryEntryRaw*)&block.data[i];
            if (ientry->entrysize == 0)
                break;

            entryfreesize = ientry->freesize();
            
            // fits between?
            if (entryfreesize >= entry.entrysize)
            {
                ientry->entrysize -= entryfreesize;
                entry.entrysize = entryfreesize;
                entry.write(&block.data[i + ientry->entrysize]);
                return blockwrite(block);
            }

            i += ientry->entrysize;
        } while (i < BLOCKSIZE);

        // fits at the end of the block?
        if (i - entryfreesize < BLOCKSIZE)
        {
            ientry->entrysize -= entryfreesize;
            entry.write(&block.data[i + ientry->entrysize]);
            return blockwrite(block);
        }
    }

    // we will need a new block
    if (blockalloc(inode, block))
    {
        entry.write(block.data);
        return blockwrite(block);
    }

    return false;
}

bool ApeFileSystem::directoryfindentry(ApeInode& inode, const string& name, ApeDirectoryEntry& entry)
{
    ApeBlock block;
    
    int n = -1;
    while (blockread(inode, ++n, block))
    {
        ApeDirectoryEntryRaw *ientry;
        int entryfreesize;
        int i = 0;

        do
        {
            ientry = (ApeDirectoryEntryRaw *)&block.data[i];
            if (ientry->entrysize == 0)
                break;

            if (ientry->namelen == name.length())
            {
                entry.read(ientry);
                if (entry.name == name)
                    return true;
            }

            i += ientry->entrysize;
        } while (i < BLOCKSIZE);
    }

    return false;
}

bool ApeFileSystem::directoryremoveentry(ApeInode& inode, const string& name)
{
    return false;
}

inline uint16_t ApeDirectoryEntryRaw::realsize() const
{
    return sizeof(ApeDirectoryEntryRaw) + namelen + 1;
}

inline uint16_t ApeDirectoryEntryRaw::freesize() const
{
    return entrysize - realsize();
}

inline bool ApeDirectoryEntryRaw::isdirectory() const
{
    return (flags & APEFLAG_DIRECTORY) != 0;
}

inline bool ApeDirectoryEntryRaw::isfile() const
{
    return (flags & APEFLAG_FILE) != 0;
}

void ApeDirectoryEntry::read(void* buffer)
{
    memcpy(this, buffer, sizeof(ApeDirectoryEntryRaw));
    name.assign((char*)buffer + sizeof(ApeDirectoryEntryRaw));
}

void ApeDirectoryEntry::write(void* buffer)
{
    memcpy(buffer, this, sizeof(ApeDirectoryEntryRaw));
    memcpy((uint8_t*)buffer + sizeof(ApeDirectoryEntryRaw), name.c_str(), namelen + 1);
}

