//
//  ResourcesManager.h
//  TestFileManager
//
//  Created by Stanislav on 13.12.13.
//  Copyright (c) 2013 Redsteep. All rights reserved.
//

#pragma once

#include <string>

class ResourcesManagerImpl;

class ResourcesManager
{
public:
    static ResourcesManager* sharedManager();
    
    void reset();
    
    void addRootFolder(const std::string& rootFolder);
    void addArchive(const std::string& archivePath);
    size_t readData(const char* filename, void* buffer, int size);
    
private:
    std::unique_ptr<ResourcesManagerImpl> pImpl;
    
    ResourcesManager();
    ResourcesManager(const ResourcesManager &);
    ResourcesManager &operator=(const ResourcesManager &);
};

