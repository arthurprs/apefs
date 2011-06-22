#include "apefilesystem.h"
#include <assert.h>
#include <math.h>

void ApeBlock::fill(uint8_t fillbyte)
{
    memset(&data, fillbyte, BLOCKSIZE);
}

bool ApeInode::isdirectory() const
{
    return (flags & APEFLAG_DIRECTORY) != 0;
}

bool ApeInode::isfile() const
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

bool ApeFileSystem::open(const string& fspath)
{
    close();
    file_.open(fspath.c_str(), ios::in | ios::out | ios::binary);
    if (!file_.good())
        return false;

    // read superblock
    file_.read((char*)&superblock_, sizeof(ApeSuperBlock));

    // verify if it's valid
    if (strncmp("apefs", superblock_.magic, 5) != 0)
        return false;

    // set the offsets
	inodesbitmapoffset_ = sizeof(ApeSuperBlock);
	blocksbitmapoffset_ = inodesbitmapoffset_ + superblock_.inodemaps * BLOCKSIZE;
	inodesoffset_ = blocksbitmapoffset_ + superblock_.blockmaps * BLOCKSIZE;
	blocksoffset_ = inodesoffset_ + superblock_.inodeblocks * BLOCKSIZE;

    // read bitmaps into memory
    int buffersize = max(superblock_.blockmaps, (uint32_t)superblock_.inodemaps) * BLOCKSIZE;
    char *buffer = new char[buffersize];

	file_.seekp(inodesbitmapoffset_);
	buffersize = superblock_.inodemaps * BLOCKSIZE;
    file_.read(buffer, buffersize);
    inodesbitmap_.frombuffer(buffer, buffersize);

	file_.seekp(blocksbitmapoffset_);
    buffersize = superblock_.blockmaps * BLOCKSIZE;
    file_.read(buffer, buffersize);
    blocksbitmap_.frombuffer(buffer, buffersize);

    delete[] buffer;

    return file_.good();
}

bool ApeFileSystem::close()
{
    file_.close();
    return file_.good();
}

ApeFile::ApeFile(ApeFileSystem& owner)
    : position(0), inodenum(INVALIDINODE), owner_(owner)
{
}

bool ApeFile::open(const string& filepath, ApeFileMode mode)
{
    return owner_.fileopen(filepath, mode, *this);
}

bool ApeFile::good() const
{
    return (inodenum != INVALIDINODE);
}

void ApeFile::close()
{
    inodenum = INVALIDINODE;
    position = 0;
}

uint32_t ApeFile::read(void* buffer, uint32_t size)
{
    return owner_.fileread(*this, buffer, size);
}

bool ApeFile::seek(ApeFileSeekMode seekmode, int32_t offset)
{
    return owner_.fileseek(*this, seekmode, offset);
}

uint32_t ApeFile::size() const
{
    return owner_.filesize(*this);
}

uint32_t ApeFile::tell() const
{
    return position;
}

uint32_t ApeFile::write(const void* buffer, uint32_t size)
{
    return owner_.filewrite(*this, buffer, size);
}

ApeFile::~ApeFile()
{
    close();
}

bool ApeFileSystem::blockalloc(ApeBlock& block)
{
    uint32_t freebit = blocksbitmap_.findunsetbit();
    if (freebit == NOBIT)
        return false;

    blocksbitmap_.setbit(freebit);
    block.num = freebit;

	file_.seekg(blocksbitmapoffset_);
	file_.write((char*)blocksbitmap_.bits(), blocksbitmap_.size());
    return file_.good();
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

    if (blockpos == 1024)
    {
        if (inode.blocks[8] == INVALIDBLOCK)
        {
            // create new indirect block
            if (!blockalloc(iblock))
                return false;
            inode.blocks[8] = iblock.num;
            iblock.fill(0xFF); // fill with invalid blocks
            if (!inodewrite(inode) || !blockwrite(iblock))
            {
                inode.blocks[8] = INVALIDBLOCK;
                return false;
            }
        }
        else
        {
            // access indirect block
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
        {
            // create new double-indirect block
            if (!blockalloc(diblock))
                return false;
            inode.blocks[9] = diblock.num;
            diblock.fill(0xFF); // fill with invalid blocks
            if (!inodewrite(inode) || !blockwrite(diblock))
            {
                inode.blocks[9] = INVALIDBLOCK;
                return false;
            }
        }
        else
        {
            // access double-indirect block
            if (!blockread(inode.blocks[9], diblock))
                return false;
        }

        if (diblock.data[blockpos / 1024] == INVALIDBLOCK)
        {
            // create new indirect block
            if (!blockalloc(iblock))
                return false;
            iblock.fill(0xFF); // fill with invalid blocks
            diblock.data[blockpos / 1024] = iblock.num;
            if (!blockwrite(diblock) || !blockwrite(iblock))
                return false;
        }
        else
        {
            // access indirect block
            if (!blockread(diblock.data[blockpos / 1024], iblock))
                return false;
        }

        iblock.data[blockpos % 1024] = block.num;
        return blockwrite(iblock);
    }

    return false;
}

bool ApeFileSystem::blockfree(blocknum_t blocknum)
{
    if (!blocksbitmap_.unsetbit(blocknum))
		return false;

	file_.seekg(blocksbitmapoffset_);
	file_.write((char*)blocksbitmap_.bits(), blocksbitmap_.size());
	return file_.good();
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
    ApeInode inode;
    if (directoryopen(path, inode) && inode.size == 0)
    {
        ApeInode parent;
        if (!directoryopen(extractdirectory(path), parent))
            return false;

        return directoryremoveentry(parent, extractfilename(path));
    }
    return false;
}

bool ApeFileSystem::directoryenum(const string& path, vector<ApeDirectoryEntry>& entries)
{
    ApeInode inode;
    if (!directoryopen(path, inode))
        return false;

    ApeBlock block;

    int n = -1;
    while (blockread(inode, ++n, block))
    {
        unsigned int i = 0;

        do
        {
            ApeDirectoryEntryRaw* ientry = (ApeDirectoryEntryRaw*)&block.data[i];
            if (ientry->entrysize == 0)
                break;

            ApeDirectoryEntry entry;
            entry.read(ientry);

            entries.push_back(entry);

            i += ientry->entrysize;
        }
        while (i < BLOCKSIZE);
    }

    return false;
}

bool ApeFileSystem::directoryexists(const string& path)
{
    ApeInode inode;
    return directoryopen(path, inode);
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
    inode.flags = APEFLAG_DIRECTORY;
    if (!inodewrite(inode))
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
    return (inodeopen(path, inode) && inode.isdirectory());
}

bool ApeFileSystem::filedelete(const string& filepath)
{
    ApeInode inode;
    if (inodeopen(filepath, inode) && inode.isfile())
    {
        ApeInode parent;
        if (!directoryopen(extractdirectory(filepath), parent))
            return false;

        return directoryremoveentry(parent, extractfilename(filepath));
    }
    return false;
}

bool ApeFileSystem::fileopen(const string& filepath, ApeFileMode mode, ApeFile& file)
{
    ApeInode inode;

    switch (mode)
    {
    case APEFILE_APPEND:
        if (inodeopen(filepath, inode) && inode.isfile())
        {
            file.inodenum = inode.num;
            file.position = inode.size;
            return true;
        }
        break;

    case APEFILE_OPEN:
        if (inodeopen(filepath, inode) && inode.isfile())
        {
            file.inodenum = inode.num;
            file.position = 0;
            return true;
        }
        break;

    case APEFILE_CREATE:
        if (inodealloc(inode))
        {
            inode.flags = APEFLAG_FILE;
            if (inodewrite(inode))
            {
                ApeInode parent;
                if (directoryopen(extractdirectory(filepath), parent))
                {
                    ApeDirectoryEntry entry;
                    entry.inodenum = inode.num;
                    entry.flags = APEFLAG_FILE;
                    entry.name = extractfilename(filepath);

                    if (directoryaddentry(parent, entry))
                    {
                        file.inodenum = inode.num;
                        file.position = 0;
                        return true;
                    }
                }
            }
        }
        break;

    case APEFILE_CLOSE:
        break;
    }

    file.inodenum = INVALIDINODE;
    file.position = 0;
    return false;
}

uint32_t ApeFileSystem::fileread(ApeFile& file, void* buffer, uint32_t size)
{
    if (!file.good())
        return 0;

    ApeInode inode;
    ApeBlock block;
    uint32_t bytesread;

    if (!inoderead(file.inodenum, inode))
        return false;

    bytesread = 0;
    while (bytesread < size && file.position < inode.size)
    {
        uint32_t bytestoread = min(BLOCKSIZE - (file.position % BLOCKSIZE), min(inode.size - file.position, size - bytesread));
        if (!blockread(inode, file.position / BLOCKSIZE, block))
            return 0;
        memcpy(&((uint8_t*)buffer)[bytesread], &block.data[file.position % BLOCKSIZE], bytestoread);
        file.position += bytestoread;
        bytesread += bytestoread;
    }
    return bytesread;
}

uint32_t ApeFileSystem::filewrite(ApeFile& file, const void* buffer, uint32_t size)
{
    if (!file.good())
        return false;

    ApeInode inode;
    if (!inoderead(file.inodenum, inode))
        return false;

    ApeBlock block;
    uint32_t byteswrote = 0;

    while (byteswrote < size)
    {
        if (file.position / BLOCKSIZE >= inode.blockscount)
        {
            // file grow
            if (!blockalloc(inode, block))
                return 0;
        }
        else
        {
            // get the corresponding block
            if (!blockread(inode, file.position / BLOCKSIZE, block))
                return 0;
        }
        uint32_t bytestowrite = min(BLOCKSIZE - (file.position % BLOCKSIZE), size - byteswrote);
        memcpy(&block.data[file.position % BLOCKSIZE], &((uint8_t*)buffer)[byteswrote], bytestowrite);
        if (!blockwrite(block))
            return false;
        file.position += bytestowrite;
        byteswrote += bytestowrite;

        if (file.position > inode.size)
        {
            inode.size = file.position;
            if (!inodewrite(inode))
                return 0; // that's a problem D:
        }
    }
    return byteswrote;
}

uint32_t ApeFileSystem::filesize(const ApeFile& file)
{
    ApeInode inode;
    if (!inoderead(file.inodenum, inode))
        return 0;
    return inode.size;
}

bool ApeFileSystem::fileseek(ApeFile& file, ApeFileSeekMode seekmode, int32_t offset)
{
    if (!file.good())
        return false;

    ApeInode inode;
    if (!inoderead(file.inodenum, inode))
        return false;

    switch (seekmode)
    {
    case APESEEK_CUR:
        if (file.position + offset >= 0 && file.position + offset < inode.size)
        {
            file.position += offset;
            return true;
        }
        break;

    case APESEEK_END:
        if (offset <= 0 && inode.size + offset >= 0)
        {
            file.position = inode.size + offset;
            return true;
        }
        break;

    case APESEEK_SET:
        if (offset >= 0 && (uint32_t)offset < inode.size)
        {
            file.position = offset;
            return true;
        }
        break;
    }

    return false;
}

uint32_t ApeFileSystem::tell(const ApeFile& file)
{
    return file.position;
}

void ApeFileSystem::fileclose(ApeFile& file)
{
    file.close();
}


bool ApeFileSystem::fileexists(const string& filepath)
{
    ApeInode inode;
    return (inodeopen(filepath, inode) && inode.isfile());
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

	file_.seekg(inodesbitmapoffset_);
	file_.write((char*)inodesbitmap_.bits(), inodesbitmap_.size());
    return file_.good();
}

bool ApeFileSystem::inodefree(inodenum_t inodenum)
{
    if (!inodesbitmap_.unsetbit(inodenum))
		return false;

	file_.seekg(inodesbitmapoffset_);
	file_.write((char*)inodesbitmap_.bits(), inodesbitmap_.size());
	return file_.good();
}

bool ApeFileSystem::inoderead(inodenum_t inodenum, ApeInode& inode)
{
    file_.seekp(inodesoffset_ + inodenum * sizeof(ApeInodeRaw));
    file_.read((char*)&inode, sizeof(ApeInodeRaw));
    return file_.good();
}

bool ApeFileSystem::inodewrite(ApeInode& inode)
{
    assert(inode.flags != 0);
    file_.seekp(inodesoffset_ + inode.num * sizeof(ApeInodeRaw));
    file_.write((char*)&inode, sizeof(ApeInodeRaw));
    return file_.good();
}

bool ApeFileSystem::inodeopen(const string& path, ApeInode& inode)
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
        if (i + 1 < parsedpath.size() && !tempentry.isdirectory())
            return false;
        if (!inoderead(tempentry.inodenum, inode))
            return false;
    }

    return true;
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
	inodesbitmapoffset_ = sizeof(ApeSuperBlock);
	blocksbitmapoffset_ = inodesbitmapoffset_ + superblock_.inodemaps * BLOCKSIZE;
	inodesoffset_ = blocksbitmapoffset_ + superblock_.blockmaps * BLOCKSIZE;
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
    inodesbitmap_.reserve(superblock_.inodemaps * BLOCKSIZE);
    inodesbitmap_.unsetall();
	blocksbitmap_.reserve(superblock_.blockmaps * BLOCKSIZE);
    blocksbitmap_.unsetall();

    // create root folder
    ApeInode rootinode;
    if (!inodealloc(rootinode))
        return false;
    assert(rootinode.num == 0);
    rootinode.flags = APEFLAG_DIRECTORY;
    return inodewrite(rootinode);
}


uint32_t ApeFileSystem::size() const
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

string ApeFileSystem::joinpath(const string& p1, const string& p2)
{
    if (p1.length() > 0 && p1[p1.length() - 1] == '/')
        return p1 + p2;
    else
        return p1 + "/" + p2;
}

string ApeFileSystem::extractdirectory(const string &path)
{
    unsigned int sep = path.rfind('/');
    if (sep != string::npos)
    {
        return path.substr(0, sep + 1);
    }
    return "";
}

string ApeFileSystem::extractfilename(const string &path)
{
    unsigned int sep = path.rfind('/');
    if (sep != string::npos)
    {
        return path.substr(sep + 1);
    }
    return "";
}

bool ApeFileSystem::directoryaddentry(ApeInode& inode, ApeDirectoryEntry& entry)
{
    assert(entry.flags != 0);
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
        unsigned int i = 0;
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
                {
                    inode.size -= pentry->freesize();
                    pentry->entrysize = pentry->realsize();
                }
                inode.size += entry.entrysize;
                entry.write(&block.data[BLOCKSIZE - freesize]);
                return inodewrite(inode) && blockwrite(block);
            }
        }
    }

    // we will need a new block
    if (blockalloc(inode, block))
    {
        block.fill(0); // fill null entries
        entry.write(&block.data[0]);
        inode.size += entry.entrysize;
        return inodewrite(inode) && blockwrite(block);
    }

    return false;
}

bool ApeFileSystem::directoryfindentry(ApeInode& inode, const string& name, ApeDirectoryEntry& entry)
{
    ApeBlock block;

    int n = -1;
    while (blockread(inode, ++n, block))
    {
        unsigned int i = 0;

        do
        {
            ApeDirectoryEntryRaw* ientry = (ApeDirectoryEntryRaw*)&block.data[i];
            if (ientry->entrysize == 0)
                break;

            if (ientry->namelen == name.length())
            {
                entry.read(ientry);
                if (entry.name == name)
                    return true;
            }

            i += ientry->entrysize;
        }
        while (i < BLOCKSIZE);
    }

    return false;
}

bool ApeFileSystem::directoryremoveentry(ApeInode& inode, const string& name)
{
    ApeBlock block;
    int n = -1;

    while (blockread(inode, ++n, block))
    {
        unsigned int i = 0;
        ApeDirectoryEntryRaw *pentry = NULL;

        do
        {
            ApeDirectoryEntryRaw *ientry = (ApeDirectoryEntryRaw*)&block.data[i];
            if (ientry->entrysize == 0)
                break;

            if (ientry->namelen == name.length())
            {
                ApeDirectoryEntry entry;
                entry.read(ientry);
                if (entry.name == name)
                {
                    if (pentry)
                    {
                        // if there is a previous entry increase its size
                        pentry->entrysize += ientry->entrysize;
                    }
                    else
                    {
                        // first entry of the block
                        ApeDirectoryEntryRaw *nentry = (ApeDirectoryEntryRaw*)&block.data[i + ientry->entrysize];
                        if (nentry->entrysize != 0)
                        {
                            // replace current with the next entry
                            nentry->entrysize += ientry->entrysize;
                            memcpy(ientry, nentry, nentry->realsize());
                        }
                        else
                        {
                            // the first and final entry of the block
                            // just mark as empty
                            inode.size -= ientry->entrysize;
                            ientry->entrysize = 0;
                            if (!inodewrite(inode))
                                return false;
                        }
                    }
                    return blockwrite(block);
                }
            }

            i += ientry->entrysize;
            pentry = ientry;
        }
        while (i < BLOCKSIZE);
    }

    return false;
}

uint16_t ApeDirectoryEntryRaw::realsize() const
{
    return sizeof(ApeDirectoryEntryRaw) + namelen + 1;
}

uint16_t ApeDirectoryEntryRaw::freesize() const
{
    return entrysize - realsize();
}

bool ApeDirectoryEntryRaw::isdirectory() const
{
    return (flags & APEFLAG_DIRECTORY) != 0;
}

bool ApeDirectoryEntryRaw::isfile() const
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

