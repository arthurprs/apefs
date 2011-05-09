#include "apefilesystem.h"

inline bool ApeInode::isfolder() const
{
    return flags & APEFLAG_FOLDER;
}

inline bool ApeInode::isfile() const
{
    return flags & APEFLAG_FILE;
}

inline uint16_t ApeInode::unref()
{
    return --references;
}

inline uint16_t ApeInode::ref()
{
    return ++references;
}

ApeFileSystem::ApeFileSystem()
{

}

ApeFileSystem::~ApeFileSystem()
{
    close();
}

bool ApeFileSystem::open(const string &fsfilepath)
{
    file_.open(fsfilepath.c_str(), ios::in | ios::out | ios::binary | ios::app);
    return !file_.fail();
}

bool ApeFileSystem::close()
{
    file_.close();
    return !file_.fail();
}

ApeFile::ApeFile(ApeFileMode mode, ApeInode& inode, ApeFileSystem& owner)
    : inode_(inode), owner_(owner), mode_(mode)
{
    inode_.ref();

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

    inode_.unref();
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
}

ApeFile::~ApeFile()
{
    close();
}


ApeDirectory::ApeDirectory(ApeInode& inode, ApeFileSystem& owner)
    : inode_(inode), owner_(owner)
{
}

bool ApeDirectory::addentry(const string& name, inodenum_t entryinode)
{

}

bool ApeDirectory::findentry(const string& name, inodenum_t& entryinode)
{

}

inline bool ApeDirectory::isempty()
{
    return inode_.size == 0;
}

bool ApeDirectory::removeentry(const string& name)
{
}


bool ApeFileSystem::blockalloc(ApeInode& inode, ApeBlock& block)
{
}

bool ApeFileSystem::blockfree(blocknum_t blocknum)
{

}

bool ApeFileSystem::blockread(blocknum_t blocknum, ApeBlock& block)
{
	file_.seekp(blocksoffset + blocknum * BLOCKSIZE);
	block.num = blocknum;
	block.dirty = false;
    file_.read((char*)&block.data, BLOCKSIZE);
    return !file_.fail();
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

bool ApeFileSystem::blockwrite(const ApeBlock& block)
{
}

void ApeFileSystem::defrag()
{
}

bool ApeFileSystem::directorydelete(const string& filepath)
{
}

bool ApeFileSystem::directoryexists(const string& filepath)
{
}

bool ApeFileSystem::directoryopen(const string& directorypath, ApeDirectory& directory)
{
}

bool ApeFileSystem::filedelete(const string& filepath)
{
}

bool ApeFileSystem::fileexists(const string& filepath)
{
}

bool ApeFileSystem::fileopen(const string& filepath, ApeFileMode mode, ApeFile& file)
{
}

bool ApeFileSystem::inodealloc(ApeInode& inode)
{
}

bool ApeFileSystem::inodefree(inodenum_t inodeid)
{
}

bool ApeFileSystem::inoderead(inodenum_t inodeid, ApeInode& inode)
{
}

bool ApeFileSystem::inodewrite(const ApeInode& inode)
{
}

