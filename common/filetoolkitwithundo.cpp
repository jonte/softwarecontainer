#include "filetoolkitwithundo.h"

#include "directorycleanuphandler.h"
#include "filecleanuphandler.h"
#include "mountcleanuphandler.h"

namespace softwarecontainer {

FileToolkitWithUndo::~FileToolkitWithUndo()
{
    bool success = true;
    // Clean up all created directories, files, and mount points
    for(auto *it : m_cleanupHandlers)
    {
        if(isError(it->clean())) {
            success = false;
        }
        delete it;
    }

    if(!success) {
        log_error() << "One or more cleanup handlers returned error status, please check the log";
    }
}

ReturnCode FileToolkitWithUndo::createParentDirectory(const std::string &path)
{
    log_debug() << "Creating parent directories for " << path;
    std::string parent = parentPath(path);
    if (!isDirectory(parent) && !parent.empty()) {
        if(isError(createDirectory(parent))) {
            log_error() << "Could not create directory " << parent;
            return ReturnCode::FAILURE;
        }
    }
    return ReturnCode::SUCCESS;
}

ReturnCode FileToolkitWithUndo::createDirectory(const std::string &path)
{
    if (isDirectory(path)) {
        return ReturnCode::SUCCESS;
    }

    if(isError(createParentDirectory(path))) {
        log_error() << "Couldn't create parent directory for " << path;
        return ReturnCode::FAILURE;
    }

    if (mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
        log_error() << "Could not create directory " << path << " - Reason : " << strerror(errno);
        return ReturnCode::FAILURE;
    }

    m_cleanupHandlers.push_back(new DirectoryCleanUpHandler(path));
    log_debug() << "Created directory " << path;

    return ReturnCode::SUCCESS;
}

std::string FileToolkitWithUndo::tempDir(std::string templ)
{
    char *dir = const_cast<char*>(templ.c_str());
    dir = mkdtemp(dir);
    if (dir == nullptr) {
        log_warning() << "Failed to create buffered Directory: " << strerror(errno);
        return nullptr;
    }

    m_cleanupHandlers.push_back(new DirectoryCleanUpHandler(templ));

    return std::string(dir);
}

ReturnCode FileToolkitWithUndo::bindMount(const std::string &src, const std::string &dst, bool readOnly, bool enableWriteBuffer)
{
    unsigned long flags = MS_BIND;
    std::string fstype;
    const void *data = nullptr;
    int mountRes;
    log_debug() << "Bind-mounting " << src << " in " << dst << ", flags: " << flags;

    if(enableWriteBuffer) {
        std::string upperDir = tempDir("/tmp/sc-bindmount-upper-XXXXXX");
        std::string workDir = tempDir("/tmp/sc-bindmount-work-XXXXXX");
        fstype.assign("overlay");

        std::ostringstream os;
        os << "lowerdir=" << src << ",upperdir=" << upperDir << ",workdir=" << workDir;
        data = os.str().c_str();

        mountRes = mount("overlay", dst.c_str(), fstype.c_str(), flags, data);
    } else {
        mountRes = mount(src.c_str(), dst.c_str(), fstype.c_str(), flags, data);
    }

    if (mountRes == 0) {
        log_verbose() << "Bind-mounted folder " << src << " in " << dst;
        m_cleanupHandlers.push_back(new MountCleanUpHandler(dst));
    } else {
        log_error() << "Could not mount into container: src=" << src
                    << " , dst=" << dst << " err=" << strerror(errno);
        return ReturnCode::FAILURE;
    }

    if (readOnly) {
        flags = MS_REMOUNT | MS_RDONLY | MS_BIND;

        log_debug() << "Re-mounting read-only" << src << " in "
                    << dst << ", flags: " << flags;
        mountRes = mount(src.c_str(), dst.c_str(), fstype.c_str(), flags, data);
        if (mountRes != 0) {
            // Failure
            log_error() << "Could not re-mount " << src << " , read-only on "
                        << dst << " err=" << strerror(errno);
            return ReturnCode::FAILURE;
        }
    }
    return ReturnCode::SUCCESS;
}

ReturnCode FileToolkitWithUndo::overlayMount(
          const std::string &lower
        , const std::string &upper
        , const std::string &work
        , const std::string &dst)
{
    std::string fstype = "overlay";
    unsigned long flags = MS_BIND;

    if ((createDirectory(lower) && ReturnCode::FAILURE)
        || (createDirectory(upper) && ReturnCode::FAILURE)
        || (createDirectory(work) && ReturnCode::FAILURE))
    {
        log_error() << "Failed to create lower/upper/work directory for overlayMount. lower=" <<
                       lower << ", upper=" << upper << ", work=" << work;
        return ReturnCode::FAILURE;
    }

    std::string mountoptions = StringBuilder() << "lowerdir=" << lower
                                               << ",upperdir=" << upper
                                               << ",workdir=" << work;

    int mountRes = mount("overlay", dst.c_str(), fstype.c_str(), flags, mountoptions.c_str());

    if (mountRes == 0) {
        log_verbose() << "overlayMounted folder " << lower << " in " << dst;
        m_cleanupHandlers.push_back(new MountCleanUpHandler(dst));
        m_cleanupHandlers.push_back(new DirectoryCleanUpHandler(upper));
        m_cleanupHandlers.push_back(new DirectoryCleanUpHandler(work));
    } else {
        log_error() << "Could not mount into container: lower=" << lower
                    << " , dst=" << dst << " err=" << strerror(errno);
        return ReturnCode::FAILURE;
    }

    return ReturnCode::SUCCESS;
}

ReturnCode FileToolkitWithUndo::createSharedMountPoint(const std::string &path)
{
    auto mountRes = mount(path.c_str(), path.c_str(), "", MS_BIND, nullptr);
    assert(mountRes == 0);
    mountRes = mount(path.c_str(), path.c_str(), "", MS_UNBINDABLE, nullptr);
    assert(mountRes == 0);
    mountRes = mount(path.c_str(), path.c_str(), "", MS_SHARED, nullptr);
    assert(mountRes == 0);
    m_cleanupHandlers.push_back(new MountCleanUpHandler(path));
    log_debug() << "Created shared mount point at " << path;

    return ReturnCode::SUCCESS;
}

ReturnCode FileToolkitWithUndo::writeToFile(const std::string &path, const std::string &content)
{
    auto ret = softwarecontainer::writeToFile(path, content);
    if (isError(ret)) {
        return ret;
    }
    m_cleanupHandlers.push_back(new FileCleanUpHandler(path));
    log_debug() << "Successfully wrote to " << path;
    return ReturnCode::SUCCESS;
}

ReturnCode FileToolkitWithUndo::createSymLink(const std::string &source, const std::string &destination)
{
    log_debug() << "creating symlink " << source << " pointing to " << destination;

    createDirectory(parentPath(source));

    if (symlink(destination.c_str(), source.c_str()) == 0) {
        m_cleanupHandlers.push_back(new FileCleanUpHandler(source));
        log_debug() << "Successfully created symlink from " << source << " to " << destination;
    } else {
        log_error() << "Error creating symlink " << destination
                    << " pointing to " << source << ". Error: "
                    << strerror(errno);
        return ReturnCode::FAILURE;
    }
    return ReturnCode::SUCCESS;
}

}