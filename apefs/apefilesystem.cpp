#include "apefilesystem.h"
#include <assert.h>
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

uint32_t ApeFile::read(void* buffer, uint32_t size)
{
    ApeBlock block;
    uint32_t bytesread;

    if (mode_ == APEFILE_CLOSE)
        return 0;

    bytesread = 0;
    while (bytesread < size && position_ < inode_.size)
    {
		uint32_t bytestoread = min(BLOCKSIZE - (position_ % BLOCKSIZE), inode_.size - position_);
        if (!owner_.blockread(inode_, position_ / BLOCKSIZE, block))
            return 0;
        memcpy(&((uint8_t*)buffer)[bytesread], &block.data[position_ % BLOCKSIZE], bytestoread);
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
    if (mode_ == APEFILE_CLOSE)
        return 0;

	ApeBlock block;
    uint32_t byteswrote = 0;

    while (byteswrote < size && position_ < inode_.size)
    {
		if (position_ / BLOCKSIZE >= inode_.blockscount)
		{	// file grow
			if (!owner_.blockalloc(inode_, block))
				return 0;
		}
		else
		{	// get the corresponding block
			if (!owner_.blockread(inode_, position_ / BLOCKSIZE, block))
				return 0;
		}
		uint32_t bytestowrite = min(BLOCKSIZE - (position_ % BLOCKSIZE), size - byteswrote);
        memcpy(&block.data[position_ % BLOCKSIZE], &((uint8_t*)buffer)[byteswrote], bytestowrite);
		if (!owner_.blockwrite(block))
			return false;
		position_ += bytestowrite;
        byteswrote += bytestowrite;

		if (position_ > inode_.size)
			inode_.size = position_;
    }
    return byteswrote;
}

ApeFile::~ApeFile()
{
    close();
}

bool ApeFileSystem::blockalloc(ApeBlock& block, uint8_t fillbyte)
{
	uint32_t freebit = blocksbitmap_.findunsetbit();
    if (freebit == NOBIT)
        return false;

    blocksbitmap_.setbit(freebit);
    memset(&block, fillbyte, sizeof(ApeBlock));
    block.num = freebit;

	return blockwrite(block);
}

bool ApeFileSystem::blockalloc(ApeInode& inode, ApeBlock& block)
{
	// TODO: free in case of a failure
    if (!blockalloc(block))
		return false;

	// add entry to inode
	if (inode.blockscount < 8)
	{
		inode.blocks[inode.blockscount++] = block.num;
		if (!inodewrite(inode))
		{
			inode.blockscount--;
			return false;
		}
		return true;
	}
	
	// offset of the new block
	uint32_t blockpos = inode.blockscount - 8; 
    ApeBlock iblock;
	
    if (blockpos = 1024)
    {
		if (inode.blocks[8] == INVALIDBLOCK)
		{	// create new indirect block
			if (!blockalloc(iblock, 0xFF))
				return false;
			inode.blocks[8] = iblock.num;
			if (!inodewrite(inode))
			{
				inode.blocks[8] = INVALIDBLOCK;
				return false;
			}
		}
		else
		{	// access indirect block
			if (!blockread(inode.blocks[8], iblock))
				return false;
		}

		iblock.data[blockpos] = block.num;
		return blockwrite(iblock);
	}
    else
    {
		ApeBlock diblock;
		if (inode.blocks[9] == INVALIDBLOCK)
		{	// create new double-indirect block
			if (!blockalloc(diblock, 0xFF))
				return false;
			inode.blocks[9] = diblock.num;
			if (!inodewrite(inode))
			{
				inode.blocks[9] = INVALIDBLOCK;
				return false;
			}
		}
		else
		{	// access double-indirect block
			if (!blockread(inode.blocks[9], diblock))
				return false;
		}

		if (diblock.data[blockpos / 1024] == INVALIDBLOCK)
		{	// create new indirect block
			if (!blockalloc(iblock, 0xFF))
				return false;
			diblock.data[blockpos / 1024] = iblock.num;
			if (!blockwrite(diblock))
				return false;
		}
		else
		{	// access indirect block
			if (!blockread(diblock.data[blockpos / 1024], iblock))
				return false;
		}

		iblock.data[blockpos % 1024] = block.num;
		return blockwrite(iblock);
    }

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

bool ApeFileSystem::blockread(const ApeInode& inode, uint32_t blockpos, ApeBlock& block)
{
    if (blockpos >= inode.blockscount)
        return false;
    if (blockpos < 8)
        return blockread(inode.blocks[blockpos], block);

    uint32_t rpos = blockpos - 8;
    ApeBlock iblock;

    if (rpos < 1024)
    {
		if (!blockread(inode.blocks[8], iblock))
			return false;
        return blockread(iblock.data[rpos], block);
    }
    else if (rpos < 1024 * 1024)
    {
        ApeBlock diblock;
		if (!blockread(inode.blocks[9], diblock))
			return false;
        if (!blockread(diblock.data[rpos / 1024], iblock))
            return false;
        return blockread(iblock.data[rpos % 1024], block);
    }

	return false;
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
	ApeInode parent;
	
	if (!directoryopen(extractdirectory(path), parent))
		return false;


	ApeDirectoryEntry entry;
	ApeInode inode;

	// TODO: free in case of a failure
	if (!inodealloc(inode))
		return false;

	entry.inodenum = inode.num;
	entry.flags = APEFLAG_DIRECTORY;
	entry.entrysize = 0;
	entry.name = extractfilename(path);
	if (entry.name == "")
		return false;

	return directoryaddentry(parent, entry);
}

bool ApeFileSystem::directoryopen(const string& path, ApeInode& inode)
{
    vector<string> parsedpath;
    
    ApeDirectoryEntry tempentry;
    // open root inode
    if (!inoderead(0, inode))
        return false;

    if (path == "/")
        return true;

    if (!parsepath(path, parsedpath))
        return false;

    // go though all folders
    for (size_t i = 0; i < parsedpath.size(); i++)
    {
        if (!directoryfindentry(inode, parsedpath[i], tempentry))
            return false;
        if (!tempentry.isdirectory())
            return false;
        if (!inoderead(tempentry.inodenum, inode))
            return false;
    }

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
	// fill the block table with INVALIDBLOCKS
	memset(&inode.blocks, 0xFF, sizeof(inode.blocks));
    inode.num = freebit;

    return inodewrite(inode);
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
    unsigned int sep = path.find('/');
    while (sep != string::npos)
    {
        unsigned int nextsep = path.find('/', sep + 1);
        string piece = path.substr(sep + 1, nextsep - 1 - sep);
        if (piece.length() > 0)
            parsedpath.push_back(piece);
        else if (sep != path.length() - 1)
            return false;
        sep = nextsep;
    }
    return !parsedpath.empty();
}

string ApeFileSystem::extractdirectory(const string &path)
{
	int sep = path.rfind('/');
	if (sep != string::npos)
	{
		return path.substr(0, sep + 1);
	}
	return "";
}

string ApeFileSystem::extractfilename(const string &path)
{
	int sep = path.rfind('/');
	if (sep != string::npos)
	{
		return path.substr(sep + 1);
	}
	return "";
}

bool ApeFileSystem::directoryaddentry(ApeInode& inode, ApeDirectoryEntry& entry)
{
	// make sure entry is unique
	ApeDirectoryEntry dummyentry;
	if (directoryfindentry(inode, entry.name, dummyentry))
		return false;

    ApeBlock block;
    entry.namelen = entry.name.length();
    entry.entrysize = entry.realsize();
    
    int n = -1;
    while (blockread(inode, ++n, block))
    {
		int i = 0;
        ApeDirectoryEntryRaw *pentry = NULL;
		ApeDirectoryEntryRaw *ientry = (ApeDirectoryEntryRaw*)&block.data[0];
		
        while (i < BLOCKSIZE && ientry->entrysize != 0)
        {
			int entryfreesize = ientry->freesize();
            
			// fits in the free space?
            if (entryfreesize >= entry.entrysize)
            {
                ientry->entrysize -= entryfreesize;
                entry.entrysize = entryfreesize;
                entry.write(&block.data[i + ientry->entrysize]);
                return blockwrite(block);
            }

            pentry = ientry;
			i += ientry->entrysize;
			ientry = (ApeDirectoryEntryRaw*)&block.data[i];
        }

        // fits after the last (if any) entry?
		if (i < BLOCKSIZE)
		{
			int freesize = pentry ? BLOCKSIZE - (i - pentry->freesize()) : BLOCKSIZE - i;
			if (freesize >= entry.entrysize)
			{
				if (pentry)
					pentry->entrysize = pentry->realsize();
				entry.write(&block.data[BLOCKSIZE - freesize]);
				return blockwrite(block);
			}
		}
    }

    // we will need a new block
    if (blockalloc(inode, block))
    {
        entry.write(&block.data[0]);
		inode.size += entry.entrysize;
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
        int i = 0;

        do
        {
            ientry = (ApeDirectoryEntryRaw*)&block.data[i];
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

