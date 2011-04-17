#include "apefilesystem.h"

ApeFileSystem::ApeFileSystem(const string& fsfilepath)
{
    file_.open(fsfilepath.c_str(), ios::in | ios::out | ios::binary | ios::app);
}

ApeFileSystem::~ApeFileSystem()
{
    //dtor
}
