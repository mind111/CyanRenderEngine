#pragma once

#include <string>
#include <vector> 
#include <set>

namespace Cyan
{
    class Directory;
    class File;

    class FilePath
    {
        std::string m_virtual;
        std::string m_physical;
    };

    class FileTree
    {
    public:
        void createDirectory();
        void createFile();

        Directory* m_rootDir = nullptr;
        std::set<std::string> m_filePathSet;
    };

    class Directory
    {
        FilePath m_path;

        std::vector<Directory*> m_childDirectories;
        std::vector<File*> m_childFiles;
    };

    class File 
    {
        FilePath m_path;
        std::string m_name; // file path exclude parent directory's 
    };

    class Archive : public File
    {

    };

    class FileSystem
    {
    public:
        void mount(const char* physicalDirectory) { }
        bool exists(const char* virtualFilePath) { }

        static constexpr const char* m_rootDirectoryName = "/";
        std::string m_mountedPhysicalDirectory;
    };
}
