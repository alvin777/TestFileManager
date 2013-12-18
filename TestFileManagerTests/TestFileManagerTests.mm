//
//  TestFileManagerTests.m
//  TestFileManagerTests
//
//  Created by Stanislav on 12.12.13.
//  Copyright (c) 2013 Redsteep. All rights reserved.
//

#import "TestFileManagerTests.h"

#include "ResourcesManager.h"

NSString *BufferToString(const char* buffer, size_t size) {
    if (!buffer) return @"";
    
    return [[NSString alloc] initWithBytes:buffer length:size encoding:NSUTF8StringEncoding];
};

@implementation TestFileManagerTests

- (void)setUp
{
    [super setUp];
    
    // Set-up code here.
}

- (void)tearDown
{
    // Tear-down code here.
    ResourcesManager::sharedManager()->reset();
    
    [super tearDown];
}

- (void)testFileExists
{
    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
    
    
    STAssertTrue(ResourcesManager::sharedManager()->exists("test.txt"), @"");
    STAssertFalse(ResourcesManager::sharedManager()->exists("non-exising-filename"), @"");
}

- (void)testReadFile
{
    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
    
    char buffer[5];
    memset(buffer, 0, sizeof(buffer));
    int bytesRead = ResourcesManager::sharedManager()->readData("test.txt", &buffer, sizeof(buffer));
    STAssertEquals(bytesRead, 4, @"");
    STAssertEqualObjects(@(buffer), @"test", @"");
}

- (void)testReadCompressedFileInZip
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String]);
    
    char buffer[5] = {0};
    int bytesRead = ResourcesManager::sharedManager()->readData("test_compressed.txt", &buffer, sizeof(buffer));
    STAssertEquals(bytesRead, 4, @"");
    STAssertEqualObjects(@(buffer), @"test", @"");
}

- (void)testReadFileInFolder
{
    ResourcesManager::sharedManager()->addRootFolder([[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"res"] UTF8String]);
    
    char buffer[100] = {0};
    ResourcesManager::sharedManager()->readData("res/file_in_folder.txt", &buffer, sizeof(buffer));
    STAssertEqualObjects(@(buffer), @"file_in_folder", @"");

    int size = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &buffer, sizeof(buffer));
    STAssertTrue(size > 0, @"");
}

- (void)testReadFileInZipFolder
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String]);
    
    char buffer[100] = {0};
    ResourcesManager::sharedManager()->readData("res/compressed_file_in_folder.txt", &buffer, sizeof(buffer));
    STAssertEqualObjects(@(buffer), @"compressed_file_in_folder", @"");

    int size = ResourcesManager::sharedManager()->readData("compressed_file_in_folder.txt", &buffer, sizeof(buffer));
    STAssertTrue(size > 0, @"");
}

- (void)testReadFileTobuffer
{
    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
    
    size_t bytesRead = 0;
    auto buffer = ResourcesManager::sharedManager()->readData("test.txt", &bytesRead);
    STAssertEquals(bytesRead, (size_t)4, @"");
    NSString *string = [[NSString alloc] initWithBytes:buffer.get() length:bytesRead encoding:NSUTF8StringEncoding];
    STAssertEqualObjects(string, @"test", @"");
}

- (void)testReadLocalizedFile
{
    ResourcesManager::sharedManager()->addLanguageFolder("ru", "localized/ru");
    ResourcesManager::sharedManager()->addLanguageFolder("es", "localized/es");
    ResourcesManager::sharedManager()->addRootFolder([[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"lang_res"] UTF8String]);
    
    size_t bytesRead = 0;

    ResourcesManager::sharedManager()->setCurrentLanguage("ru");
    auto buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"файл в папке", @"");

    ResourcesManager::sharedManager()->setCurrentLanguage("es");
    buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"un \"file\" es en papel", @"");
}

// apk schema
- (void)testResFolderInZip
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"res" ofType:@"zip"] UTF8String], "res");
    
    size_t bytesRead = 0;
    auto buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"file_in_folder", @"");

    buffer = ResourcesManager::sharedManager()->readData("badfile.txt", &bytesRead);
    STAssertEquals(bytesRead, (size_t)0, @"");
}

- (void)testLocalizedResFolderInZip
{
    ResourcesManager::sharedManager()->addLanguageFolder("ru", "localized/ru");
    ResourcesManager::sharedManager()->addLanguageFolder("es", "localized/es");
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"lang_res" ofType:@"zip"] UTF8String], "lang_res");
    
    size_t bytesRead = 0;
    
    ResourcesManager::sharedManager()->setCurrentLanguage("ru");
    auto buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"файл в папке", @"");
    
    ResourcesManager::sharedManager()->setCurrentLanguage("es");
    buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"un \"file\" es en papel", @"");
}

- (void)testCategoryFile
{
    ResourcesManager::sharedManager()->enableTrace(true);

    ResourcesManager::sharedManager()->addCategoryFolder("small-screen", "small-screen");
    ResourcesManager::sharedManager()->addCategoryFolder("large-screen", "large-screen");
    ResourcesManager::sharedManager()->addRootFolder([[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"category_res"] UTF8String]);
    
    size_t bytesRead = 0;

    auto buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"regular version", @"");

    ResourcesManager::sharedManager()->enableCategory("small-screen");
    buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"small screen version", @"");
    
    ResourcesManager::sharedManager()->disableCategory("small-screen");
    ResourcesManager::sharedManager()->enableCategory("large-screen");

    buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"large screen version", @"");
}

- (void)testCategoryCompressedFile
{
    ResourcesManager::sharedManager()->addCategoryFolder("small-screen", "small-screen");
    ResourcesManager::sharedManager()->addCategoryFolder("large-screen", "large-screen");
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"category_res" ofType:@"zip"] UTF8String], "category_res");
    
    size_t bytesRead = 0;
    
    auto buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"regular version", @"");
    
    ResourcesManager::sharedManager()->enableCategory("small-screen");
    buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"small screen version", @"");
    
    ResourcesManager::sharedManager()->disableCategory("small-screen");
    ResourcesManager::sharedManager()->enableCategory("large-screen");
    
    buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"large screen version", @"");
}

- (void)testReadFileStream
{
    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
    
    auto stream = ResourcesManager::sharedManager()->getStream("test.txt");
    
    char buffer[3] = {0};
    int bytesRead = stream->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEqualObjects(@(buffer), @"te", @"");
                                     
    bytesRead = stream->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEqualObjects(@(buffer), @"st", @"");
}

- (void)testReadCompressedFileStream
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"test" ofType:@"zip"] UTF8String]);
    
    auto stream = ResourcesManager::sharedManager()->getStream("test.txt");
    
    char buffer[3] = {0};
    int bytesRead = stream->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEqualObjects(@(buffer), @"te", @"");
    
    bytesRead = stream->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEqualObjects(@(buffer), @"st", @"");
    
}

- (void)testReadStoredFileStream
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"test_stored" ofType:@"zip"] UTF8String]);
    
    auto stream = ResourcesManager::sharedManager()->getStream("test.txt");
    
    char buffer[3] = {0};
    int bytesRead = stream->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEqualObjects(@(buffer), @"te", @"");
    
    bytesRead = stream->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEqualObjects(@(buffer), @"st", @"");
    
}

- (void)testSearchRoots
{
    ResourcesManager::sharedManager()->setSearchByRelativePaths(true);
    ResourcesManager::sharedManager()->addRootFolder([[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"res_search"] UTF8String]);
    ResourcesManager::sharedManager()->addSearchRoot("folder1/search_root");
                                                     
    size_t bytesRead = 0;
    auto buffer = ResourcesManager::sharedManager()->readData("folder1/test.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"test", @"");
    
    ResourcesManager::sharedManager()->readData("test.txt", &bytesRead);
    STAssertEquals(bytesRead, (size_t)0, @"");
}

- (void)testSearchRootsInArchive
{
    ResourcesManager::sharedManager()->setSearchByRelativePaths(true);
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"res_search" ofType:@"zip"] UTF8String], "res_search");
    ResourcesManager::sharedManager()->addSearchRoot("folder1/search_root");
    
    size_t bytesRead = 0;
    auto buffer = ResourcesManager::sharedManager()->readData("folder1/test.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    STAssertEqualObjects(BufferToString(buffer.get(), bytesRead), @"test", @"");
    
    ResourcesManager::sharedManager()->readData("test.txt", &bytesRead);
    STAssertEquals(bytesRead, (size_t)0, @"");
}

- (void)testStreamSeekTell
{
    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
    
    auto stream = ResourcesManager::sharedManager()->getStream("test.txt");
    
    char buffer[3] = {0};
    int bytesRead = stream->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEquals(stream->tell(), 2L, @"");
    STAssertEqualObjects(@(buffer), @"te", @"");
    
    bytesRead = stream->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEquals(stream->tell(), 4L, @"");
    STAssertEqualObjects(@(buffer), @"st", @"");
    
    stream->seek(1, SEEK_SET);
    STAssertEquals(stream->tell(), 1L, @"");
    
    bytesRead = stream->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEquals(stream->tell(), 3L, @"");
    STAssertEqualObjects(@(buffer), @"es", @"");
}

- (void)testConcurrentZipStreams
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"test" ofType:@"zip"] UTF8String]);
    
    auto stream1 = ResourcesManager::sharedManager()->getStream("test.txt");
    auto stream2 = ResourcesManager::sharedManager()->getStream("test.txt");
    
    char buffer[4] = {0};
    int bytesRead = stream1->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEqualObjects(@(buffer), @"te", @"");
    
    memset(buffer, 0, sizeof(buffer));
    bytesRead = stream2->readData(&buffer, 1);
    STAssertEquals(bytesRead, 1, @"");
    STAssertEqualObjects(@(buffer), @"t", @"");
    
    memset(buffer, 0, sizeof(buffer));
    bytesRead = stream1->readData(&buffer, 2);
    STAssertEquals(bytesRead, 2, @"");
    STAssertEqualObjects(@(buffer), @"st", @"");
    
    memset(buffer, 0, sizeof(buffer));
    bytesRead = stream2->readData(&buffer, 3);
    STAssertEquals(bytesRead, 3, @"");
    STAssertEqualObjects(@(buffer), @"est", @"");
}
@end
