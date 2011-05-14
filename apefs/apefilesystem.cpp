#include "apefilesystem.h"

#include <math.h>

inline bool ApeInode::isfolder() const
{
    return (flags & APEFLAG_FOLDER) != 0;
}

inline bool ApeInode::isfile() const
{
    return flags & APEFLAG_FILE;
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


ApeDirectory::ApeDirectory(ApeInode& inode, ApeFileSystem& owner)
    : inode_(inode), owner_(owner)
{
}

bool ApeDirectory::addentry(const string& name, inodenum_t entryinode)
{
	return false;
}

bool ApeDirectory::findentry(const string& name, inodenum_t& entryinode)
{
	return false;
}

inline bool ApeDirectory::isempty()
{
    return inode_.size == 0;
}

bool ApeDirectory::removeentry(const string& name)
{
	return false;
}


bool ApeFileSystem::blockalloc(ApeInode& inode, ApeBlock& block)
{
	return false;
}

bool ApeFileSystem::blockfree(blocknum_t blocknum)
{
	return false;
}

bool ApeFileSystem::blockread(blocknum_t blocknum, ApeBlock& block)
{
	file_.seekp(blocksoffset_ + blocknum * BLOCKSIZE);
	block.num = blocknum;
	block.dirty = false;
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

bool ApeFileSystem::blockwrite(const ApeBlock& block)
{
	return false;
}

void ApeFileSystem::defrag()
{

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

bool ApeFileSystem::directoryopen(const string& path, ApeDirectory& directory)
{
	return false;
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
}

bool ApeFileSystem::inodefree(inodenum_t inodeid)
{
	return false;
}

bool ApeFileSystem::inoderead(inodenum_t inodeid, ApeInode& inode)
{
	return false;
}

bool ApeFileSystem::inodewrite(const ApeInode& inode)
{
	return false;
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

	int totalblanks = superblock_.inodemaps + superblock_.blockmaps + superblock_.inodeblocks;
	for (int i = 0; i < totalblanks; i++)
		file_.write(blank, BLOCKSIZE);

	delete[] blank;

	// create root folder
	ApeInode rootinode;
	inodealloc(rootinode);
	rootinode.flags |= APEFLAG_FOLDER;
	inodewrite(rootinode);

	return true;
}


inline uint32_t ApeFileSystem::size() const
{
	return superblock_.filesystemsize;
}

