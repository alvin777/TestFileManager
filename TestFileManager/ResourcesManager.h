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
class Stream;

class ResourcesManager
{
public:
    friend class Stream;
    
    static ResourcesManager* sharedManager();
    
    void reset();
    
    void enableTrace(bool enableTrace);
    
    void addRootFolder(const std::string& rootFolder);
    void addArchive(const std::string& archivePath, const std::string& rootFolder = "");
    
    void addLanguageFolder(const std::string& languageId, const std::string& languageFolder);
    void addCategoryFolder(const std::string& category, const std::string& categoryFolder);
    void enableCategory(const std::string& category);
    void disableCategory(const std::string& category);
    void setCurrentLanguage(const std::string& languageId);
    void setSearchByRelativePaths(bool searchByRelativePaths);
    void addSearchRoot(const std::string& searchRoot);
    
    void rebuildIndex();
    
    bool exists(const std::string& filename);
    size_t getSize(const std::string& filename);
    size_t readData(const std::string& filename, void* buffer, int size);
    std::unique_ptr<char[]> readData(const std::string& filename, size_t* bytesRead);
    
    std::unique_ptr<Stream> getStream(const std::string& filename);
    
private:
    std::unique_ptr<ResourcesManagerImpl> pImpl;
    
//    int openFile(const std::string& filename);
    size_t readData(int handle, void* buffer, int size);
    int closeFile(int handle);
    int seek (int handle, long int offset, int whence);
    long int tell(int handle);
    
    ResourcesManager();
    ResourcesManager(const ResourcesManager &);
    ResourcesManager &operator=(const ResourcesManager &);
};


class StreamImpl;

class Stream {
public:
    friend class ResourcesManager;

    ~Stream();

    size_t readData(void* buffer, int size);
    std::unique_ptr<char[]> readData(size_t* bytesRead);

    int seek (long int offset, int whence);
    long int tell();

private:
    Stream();
    Stream(const Stream&);
    Stream &operator=(const Stream&);

    Stream(int handle);
    std::unique_ptr<StreamImpl> pImpl;
};