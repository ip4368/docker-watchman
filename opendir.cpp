/* Copyright 2012-present Facebook, Inc.
 * Licensed under the Apache License, Version 2.0 */

#include "watchman.h"
#include <system_error>
#ifndef _WIN32
#include <dirent.h>
#endif
#ifdef __APPLE__
# include <sys/utsname.h>
# include <sys/attr.h>
# include <sys/vnode.h>
#endif
#include "FileDescriptor.h"

using namespace watchman;
using watchman::FileDescriptor;
using watchman::OpenFileHandleOptions;

#ifdef HAVE_GETATTRLISTBULK
// The ordering of these fields is defined by the ordering of the
// corresponding ATTR_XXX flags that are listed after each item.
// Those flags appear in a specific order in the getattrlist()
// man page.  We use FSOPT_PACK_INVAL_ATTRS to ensure that the
// kernel won't omit a field that it didn't return to us.
typedef struct {
  uint32_t len;
  attribute_set_t returned; // ATTR_CMN_RETURNED_ATTRS
  uint32_t err; // ATTR_CMN_ERROR

  /* The attribute data length will not be greater than NAME_MAX + 1
   * characters, which is NAME_MAX * 3 + 1 bytes (as one UTF-8-encoded
   * character may take up to three bytes
   */
  attrreference_t name; // ATTR_CMN_NAME
  dev_t dev;            // ATTR_CMN_DEVID
  fsobj_type_t objtype; // ATTR_CMN_OBJTYPE
  struct timespec mtime; // ATTR_CMN_MODTIME
  struct timespec ctime; // ATTR_CMN_CHGTIME
  struct timespec atime; // ATTR_CMN_ACCTIME
  uid_t uid; // ATTR_CMN_OWNERID
  gid_t gid; // ATTR_CMN_GRPID
  uint32_t mode; // ATTR_CMN_ACCESSMASK, Only the permission bits of st_mode
                 // are valid; other bits should be ignored,
                 // e.g., by masking with ~S_IFMT.
  uint64_t ino;  // ATTR_CMN_FILEID
  uint32_t link; // ATTR_FILE_LINKCOUNT or ATTR_DIR_LINKCOUNT
  off_t file_size; // ATTR_FILE_TOTALSIZE

} __attribute__((packed)) bulk_attr_item;
#endif

#ifndef _WIN32
class DirHandle : public watchman_dir_handle {
#ifdef HAVE_GETATTRLISTBULK
  FileDescriptor fd_;
  struct attrlist attrlist_;
  int retcount_{0};
  char buf_[64 * (sizeof(bulk_attr_item) + NAME_MAX * 3 + 1)];
  char *cursor_{nullptr};
#endif
  DIR *d_{nullptr};
  struct watchman_dir_ent ent_;

 public:
  explicit DirHandle(const char* path, bool strict);
  ~DirHandle() override;
  const watchman_dir_ent* readDir() override;
  int getFd() const override;
};
#endif

#ifndef _WIN32
/* Opens a directory making sure it's not a symlink */
static DIR *opendir_nofollow(const char *path)
{
  auto fd = openFileHandle(path, OpenFileHandleOptions::strictOpenDir());
# if !defined(HAVE_FDOPENDIR) || defined(__APPLE__)
  /* fdopendir doesn't work on earlier versions OS X, and we don't
   * use this function since 10.10, as we prefer to use getattrlistbulk
   * in that case */
  return opendir(path);
# else
  // errno should be set appropriately if this is not a directory
  auto d = fdopendir(fd.fd());
  if (d) {
    fd.release();
  }
  return d;
# endif
}
#endif

#ifdef HAVE_GETATTRLISTBULK
// I've seen bulkstat report incorrect sizes on kernel version 14.5.0.
// (That's OSX 10.10.5).
// Let's avoid it for major kernel versions < 15.
// Using statics here to avoid querying the uname on every opendir.
// There is opportunity for a data race the first time through, but the
// worst case side effect is wasted compute early on.
static bool use_bulkstat_by_default(void) {
  static bool probed = false;
  static bool safe = false;

  if (!probed) {
    struct utsname name;
    if (uname(&name) == 0) {
      int maj = 0, min = 0, patch = 0;
      sscanf(name.release, "%d.%d.%d", &maj, &min, &patch);
      if (maj >= 15) {
        safe = true;
      }
    }
    probed = true;
  }

  return safe;
}
#endif

#ifndef _WIN32
std::unique_ptr<watchman_dir_handle> w_dir_open(const char* path, bool strict) {
  return watchman::make_unique<DirHandle>(path, strict);
}

DirHandle::DirHandle(const char* path, bool strict) {
#ifdef HAVE_GETATTRLISTBULK
  if (cfg_get_bool("_use_bulkstat", use_bulkstat_by_default())) {
    auto opts = strict ? OpenFileHandleOptions::strictOpenDir()
                       : OpenFileHandleOptions::openDir();

    fd_ = openFileHandle(path, opts);

    auto info = fd_.getInfo();

    if (!info.isDir()) {
      throw std::system_error(ENOTDIR, std::generic_category(), path);
    }

    attrlist_ = attrlist();
    attrlist_.bitmapcount = ATTR_BIT_MAP_COUNT;
    // These field flags are listed here in the same order that they
    // are listed in the getattrlist() manpage, which is also the
    // same order that they will be emitted into the buffer, which
    // is thus the order that they must appear in bulk_attr_item.
    attrlist_.commonattr = ATTR_CMN_RETURNED_ATTRS |
      ATTR_CMN_ERROR |
      ATTR_CMN_NAME |
      ATTR_CMN_DEVID |
      ATTR_CMN_OBJTYPE |
      ATTR_CMN_MODTIME |
      ATTR_CMN_CHGTIME |
      ATTR_CMN_ACCTIME |
      ATTR_CMN_OWNERID |
      ATTR_CMN_GRPID |
      ATTR_CMN_ACCESSMASK |
      ATTR_CMN_FILEID;
    attrlist_.dirattr = ATTR_DIR_LINKCOUNT;
    attrlist_.fileattr = ATTR_FILE_TOTALSIZE | ATTR_FILE_LINKCOUNT;
    return;
  }
#endif
  d_ = strict ? opendir_nofollow(path) : opendir(path);

  if (!d_) {
    throw std::system_error(
        errno,
        std::generic_category(),
        std::string(strict ? "opendir_nofollow: " : "opendir: ") + path);
  }
}

const watchman_dir_ent* DirHandle::readDir() {
#ifdef HAVE_GETATTRLISTBULK
  if (fd_) {
    bulk_attr_item *item;

    if (!cursor_) {
      // Read the next batch of results
      int retcount;

      errno = 0;
      retcount = getattrlistbulk(
          fd_.fd(),
          &attrlist_,
          buf_,
          sizeof(buf_),
          // FSOPT_PACK_INVAL_ATTRS informs the kernel that we want to
          // include attrs in our buffer even if it doesn't return them
          // to us; we want this because we took pains to craft our
          // bulk_attr_item struct to avoid pointer math.
          FSOPT_PACK_INVAL_ATTRS);
      if (retcount == -1) {
        throw std::system_error(
            errno, std::generic_category(), "getattrlistbulk");
      }
      if (retcount == 0) {
        // End of the stream
        return nullptr;
      }

      retcount_ = retcount;
      cursor_ = buf_;
    }

    // Decode the next item
    item = (bulk_attr_item*)cursor_;
    cursor_ += item->len;
    if (--retcount_ == 0) {
      cursor_ = nullptr;
    }

    w_string_piece name{};

    if (item->returned.commonattr & ATTR_CMN_NAME) {
      ent_.d_name = ((char*)&item->name) + item->name.attr_dataoffset;
      name = w_string_piece(ent_.d_name);
    }

    if (item->returned.commonattr & ATTR_CMN_ERROR) {
      log(ERR,
          "getattrlistbulk: error while reading dir: entry ",
          name,
          strerror(item->err));

      // No name means we've got nothing useful to go on
      if (name.empty()) {
        throw std::system_error(
            item->err, std::generic_category(), "getattrlistbulk");
      }

      // Getting the name means that we can at least enumerate the dir
      // contents.
      ent_.has_stat = false;
      return &ent_;
    }

    if (name.empty()) {
      throw std::system_error(
          EIO,
          std::generic_category(),
          "getattrlistbulk didn't return a name for a directory entry!?");
    }

    if (item->returned.commonattr != (attrlist_.commonattr & ~ATTR_CMN_ERROR)) {
      log(ERR,
          "getattrlistbulk didn't return all useful stat data! returned=",
          item->returned.commonattr);
      // We can still yield the name, so we don't need to throw an exception
      // in this case.
      ent_.has_stat = false;
      return &ent_;
    }

    ent_.stat = watchman::FileInformation();

    ent_.stat.dev = item->dev;
    memcpy(&ent_.stat.mtime, &item->mtime, sizeof(item->mtime));
    memcpy(&ent_.stat.ctime, &item->ctime, sizeof(item->ctime));
    memcpy(&ent_.stat.atime, &item->atime, sizeof(item->atime));
    ent_.stat.uid = item->uid;
    ent_.stat.gid = item->gid;
    ent_.stat.mode = item->mode & ~S_IFMT;
    ent_.stat.ino = item->ino;

    switch (item->objtype) {
      case VREG:
        ent_.stat.mode |= S_IFREG;
        ent_.stat.size = item->file_size;
        ent_.stat.nlink = item->link;
        break;
      case VDIR:
        ent_.stat.mode |= S_IFDIR;
        ent_.stat.nlink = item->link;
        break;
      case VLNK:
        ent_.stat.mode |= S_IFLNK;
        ent_.stat.size = item->file_size;
        break;
      case VBLK:
        ent_.stat.mode |= S_IFBLK;
        break;
      case VCHR:
        ent_.stat.mode |= S_IFCHR;
        break;
      case VFIFO:
        ent_.stat.mode |= S_IFIFO;
        break;
      case VSOCK:
        ent_.stat.mode |= S_IFSOCK;
        break;
    }
    ent_.has_stat = true;
    return &ent_;
  }
#endif

  if (!d_) {
    return nullptr;
  }
  errno = 0;
  auto dent = readdir(d_);
  if (!dent) {
    if (errno) {
      throw std::system_error(errno, std::generic_category(), "readdir");
    }
    return nullptr;
  }

  ent_.d_name = dent->d_name;
  ent_.has_stat = false;
  return &ent_;
}

DirHandle::~DirHandle() {
  if (d_) {
    closedir(d_);
  }
}

int DirHandle::getFd() const {
#ifdef HAVE_GETATTRLISTBULK
  if (cfg_get_bool("_use_bulkstat", use_bulkstat_by_default())) {
    return fd_.fd();
  }
#endif
  return dirfd(d_);
}
#endif

/* vim:ts=2:sw=2:et:
 */
