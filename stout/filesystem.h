#pragma once

#include <filesystem> // std::filesystem::path
#include <optional>

#include "stout/event-loop.h"
#include "stout/eventual.h"
#include "uv.h"

////////////////////////////////////////////////////////////////////////

namespace stout {
namespace eventuals {
namespace filesystem {

////////////////////////////////////////////////////////////////////////

namespace detail {

////////////////////////////////////////////////////////////////////////

// Moveable, not Copyable.
class Request {
 public:
  Request() {}

  Request(const Request&) = delete;
  Request(Request&& that)
    : request_(std::move(that.request_)) {
    // Moving optional does not reset its state,
    // we have to manually do it.
    that.request_.reset();
  }

  Request& operator=(const Request&) = delete;
  Request& operator=(Request&& that) {
    if (request_.has_value()) {
      // libuv doesn't use loop in this call, so we don't have to be
      // within the event loop.
      uv_fs_req_cleanup(&*request_);
    }

    // Moving optional does not reset its state,
    // we have to manually do it.
    request_ = std::move(that.request_);
    that.request_.reset();

    return *this;
  }

  // Trying to cleanup an empty uv_req_t gives us an exception.
  ~Request() {
    if (request_.has_value()) {
      // libuv doesn't use loop in this call, so we don't have to be
      // within the event loop.
      uv_fs_req_cleanup(&*request_);
    }
  }

  // Seamless access to the fields in uv_fs_t structure.
  uv_fs_t* operator->() {
    CHECK(request_.has_value());
    return &*request_;
  }

  // Used as an adaptor to libuv functions.
  operator uv_fs_t*() {
    CHECK(request_.has_value());
    return &*request_;
  }

 private:
  // Stores request structure in stack, it gets moved around a lot,
  // optional eases the ownership control.
  std::optional<uv_fs_t> request_ = std::make_optional<uv_fs_t>();
};

////////////////////////////////////////////////////////////////////////

} // namespace detail

////////////////////////////////////////////////////////////////////////

// Moveable, not Copyable.
class File {
 public:
  // Removing default constructor breaks Windows build
  // due to how std::future is implemented.
  File() {}

  File(const File& that) = delete;

  File(File&& that)
    : descriptor_(std::move(that.descriptor_)) {
    // Moving optional does not reset its state,
    // we have to manually do it.
    that.descriptor_.reset();
  }

  File& operator=(const File& that) = delete;

  File& operator=(File&& that) {
    if (descriptor_.has_value()) {
      // No callback allows us to synchronously use this function,
      // loop is not needed in this variant.
      uv_fs_close(nullptr, detail::Request(), *descriptor_, NULL);
    }

    // Moving optional does not reset its state,
    // we have to manually do it.
    descriptor_ = std::move(that.descriptor_);
    that.descriptor_.reset();

    return *this;
  }

  ~File() {
    if (descriptor_.has_value()) {
      // No callback allows us to synchronously use this function,
      // loop is not needed in this variant.
      uv_fs_close(nullptr, detail::Request(), *descriptor_, NULL);
    }
  }

  bool IsOpen() {
    return descriptor_.has_value();
  }

  // Used as an adaptor to libuv functions.
  operator uv_file() const {
    return descriptor_.value();
  }

 private:
  // Stores file descriptor in stack, it gets moved around a lot,
  // optional eases the ownership control.
  std::optional<uv_file> descriptor_;

  // Takes ownership of the descriptor.
  File(const uv_file& descriptor)
    : descriptor_(descriptor) {}

  void MarkAsClosed() {
    descriptor_.reset();
  }

  friend auto OpenFile(
      EventLoop& loop,
      const std::filesystem::path& path,
      const int& flags,
      const int& mode);

  friend auto CloseFile(EventLoop& loop, File&& file);
};

////////////////////////////////////////////////////////////////////////

// Moveable and Copyable.
class Buffer {
 public:
  Buffer() {
    buffer_ = uv_buf_init(nullptr, 0);
  }

  Buffer(const size_t& size) {
    data_.resize(size);
    buffer_ = uv_buf_init(const_cast<char*>(data_.data()), size);
  }

  Buffer(const std::string& data)
    : data_(data) {
    buffer_ = uv_buf_init(const_cast<char*>(data_.data()), data_.size());
  }

  Buffer(const Buffer& that) {
    data_ = that.data_;
    buffer_ = uv_buf_init(const_cast<char*>(data_.data()), data_.size());
  }

  Buffer(Buffer&& that) {
    data_ = std::move(that.data_);
    buffer_ = uv_buf_init(const_cast<char*>(data_.data()), data_.size());

    that.buffer_.len = 0;
    that.buffer_.base = nullptr;
  }

  Buffer& operator=(const std::string& data) {
    data_ = data;
    buffer_ = uv_buf_init(const_cast<char*>(data_.data()), data_.size());

    return *this;
  }

  Buffer& operator=(std::string&& data) {
    data_ = std::move(data);
    buffer_ = uv_buf_init(const_cast<char*>(data_.data()), data_.size());

    return *this;
  }

  Buffer& operator=(const Buffer& that) {
    data_ = that.data_;
    buffer_ = uv_buf_init(const_cast<char*>(data_.data()), data_.size());

    return *this;
  }

  Buffer& operator=(Buffer&& that) {
    data_ = std::move(that.data_);
    buffer_ = uv_buf_init(const_cast<char*>(data_.data()), data_.size());

    that.buffer_.len = 0;
    that.buffer_.base = nullptr;

    return *this;
  }

  ~Buffer() {}

  // Extracts the data from the buffer as a universal reference.
  // Empties out the buffer inside.
  std::string&& Extract() noexcept {
    buffer_.len = 0;
    buffer_.base = nullptr;

    return std::move(data_);
  }

  size_t Size() const noexcept {
    return data_.size();
  }

  void Resize(const size_t& size) {
    data_.resize(size, 0);
    buffer_ = uv_buf_init(const_cast<char*>(data_.data()), size);
  }

  // Used as an adaptor to libuv functions.
  operator uv_buf_t*() noexcept {
    return &buffer_;
  }

 private:
  // Used for performance purposes (SSO?)
  std::string data_ = "";

  // base - ptr to data; len - size of data
  uv_buf_t buffer_ = {};
};

////////////////////////////////////////////////////////////////////////

inline auto OpenFile(
    EventLoop& loop,
    const std::filesystem::path& path,
    const int& flags,
    const int& mode) {
  struct Data {
    EventLoop& loop;
    int flags;
    int mode;
    std::filesystem::path path;

    detail::Request req;
    void* k = nullptr;
  };

  return loop.Schedule(
      "OpenFile",
      Eventual<File>()
          .context(Data{loop, flags, mode, path})
          .start([](auto& data, auto& k) mutable {
            using K = std::decay_t<decltype(k)>;

            data.k = &k;
            data.req->data = &data;

            auto error = uv_fs_open(
                data.loop,
                data.req,
                data.path.string().c_str(),
                data.flags,
                data.mode,
                [](uv_fs_t* req) {
                  auto& data = *static_cast<Data*>(req->data);
                  auto& k = *static_cast<K*>(data.k);
                  if (req->result >= 0) {
                    k.Start(File(req->result));
                  } else {
                    k.Fail(uv_strerror(req->result));
                  }
                });

            if (error) {
              static_cast<K*>(data.k)->Fail(uv_strerror(error));
            }
          }));
}

////////////////////////////////////////////////////////////////////////

inline auto OpenFile(
    const std::filesystem::path& path,
    const int& flags,
    const int& mode) {
  return OpenFile(EventLoop::Default(), path, flags, mode);
}

////////////////////////////////////////////////////////////////////////

inline auto CloseFile(EventLoop& loop, File&& file) {
  struct Data {
    EventLoop& loop;
    File file;
    detail::Request req;

    void* k = nullptr;
  };

  return loop.Schedule(
      "CloseFile",
      Eventual<void>()
          .context(Data{loop, std::move(file)})
          .start([](auto& data, auto& k) mutable {
            using K = std::decay_t<decltype(k)>;

            data.k = &k;
            data.req->data = &data;

            auto error = uv_fs_close(
                data.loop,
                data.req,
                data.file,
                [](uv_fs_t* req) {
                  auto& data = *static_cast<Data*>(req->data);
                  auto& k = *static_cast<K*>(data.k);
                  if (req->result == 0) {
                    data.file.MarkAsClosed();
                    k.Start();
                  } else {
                    k.Fail(uv_strerror(req->result));
                  };
                });

            if (error) {
              static_cast<K*>(data.k)->Fail(uv_strerror(error));
            }
          }));
}

////////////////////////////////////////////////////////////////////////

inline auto CloseFile(File&& file) {
  return CloseFile(EventLoop::Default(), std::move(file));
}

////////////////////////////////////////////////////////////////////////

inline auto ReadFile(
    EventLoop& loop,
    const File& file,
    const size_t& bytes_to_read,
    const size_t& offset) {
  struct Data {
    EventLoop& loop;
    const File& file;
    size_t bytes_to_read;
    size_t offset;
    Buffer buf;
    detail::Request req;

    void* k = nullptr;
  };

  return loop.Schedule(
      "ReadFile",
      Eventual<std::string>()
          .context(Data{loop, file, bytes_to_read, offset, bytes_to_read})
          .start([](auto& data, auto& k) mutable {
            using K = std::decay_t<decltype(k)>;

            data.k = &k;
            data.req->data = &data;

            auto error = uv_fs_read(
                data.loop,
                data.req,
                data.file,
                data.buf,
                1,
                data.offset,
                [](uv_fs_t* req) {
                  auto& data = *static_cast<Data*>(req->data);
                  auto& k = *static_cast<K*>(data.k);
                  if (req->result >= 0) {
                    k.Start(data.buf.Extract());
                  } else {
                    k.Fail(uv_strerror(req->result));
                  };
                });

            if (error) {
              static_cast<K*>(data.k)->Fail(uv_strerror(error));
            }
          }));
}

////////////////////////////////////////////////////////////////////////

inline auto ReadFile(
    const File& file,
    const size_t& bytes_to_read,
    const size_t& offset) {
  return ReadFile(EventLoop::Default(), file, bytes_to_read, offset);
}

////////////////////////////////////////////////////////////////////////

inline auto WriteFile(
    EventLoop& loop,
    const File& file,
    const std::string& data,
    const size_t& offset) {
  struct Data {
    EventLoop& loop;
    const File& file;
    Buffer buf;
    size_t offset;
    detail::Request req;

    void* k = nullptr;
  };

  return loop.Schedule(
      "WriteFile",
      Eventual<void>()
          .context(Data{loop, file, data, offset})
          .start([](auto& data, auto& k) mutable {
            using K = std::decay_t<decltype(k)>;

            data.k = &k;
            data.req->data = &data;

            auto error = uv_fs_write(
                data.loop,
                data.req,
                data.file,
                data.buf,
                1,
                data.offset,
                [](uv_fs_t* req) {
                  auto& data = *static_cast<Data*>(req->data);
                  auto& k = *static_cast<K*>(data.k);
                  if (req->result >= 0) {
                    k.Start();
                  } else {
                    k.Fail(uv_strerror(req->result));
                  };
                });

            if (error) {
              static_cast<K*>(data.k)->Fail(uv_strerror(error));
            }
          }));
}

////////////////////////////////////////////////////////////////////////

inline auto WriteFile(
    const File& file,
    const std::string& data,
    const size_t& offset) {
  return WriteFile(EventLoop::Default(), file, data, offset);
}

////////////////////////////////////////////////////////////////////////

inline auto UnlinkFile(EventLoop& loop, const std::filesystem::path& path) {
  struct Data {
    EventLoop& loop;
    std::filesystem::path path;

    detail::Request req;
    void* k = nullptr;
  };

  return loop.Schedule(
      "UnlinkFile",
      Eventual<void>()
          .context(Data{loop, path})
          .start([](auto& data, auto& k) mutable {
            using K = std::decay_t<decltype(k)>;

            data.k = &k;
            data.req->data = &data;

            auto error = uv_fs_unlink(
                data.loop,
                data.req,
                data.path.string().c_str(),
                [](uv_fs_t* req) {
                  auto& data = *static_cast<Data*>(req->data);
                  auto& k = *static_cast<K*>(data.k);
                  if (req->result == 0) {
                    k.Start();
                  } else {
                    k.Fail(uv_strerror(req->result));
                  }
                });

            if (error) {
              static_cast<K*>(data.k)->Fail(uv_strerror(error));
            }
          }));
}

////////////////////////////////////////////////////////////////////////

inline auto UnlinkFile(const std::filesystem::path& path) {
  return UnlinkFile(EventLoop::Default(), path);
}

////////////////////////////////////////////////////////////////////////

inline auto MakeDirectory(
    EventLoop& loop,
    const std::filesystem::path& path,
    const int& mode) {
  struct Data {
    EventLoop& loop;
    std::filesystem::path path;
    int mode;

    detail::Request req;
    void* k = nullptr;
  };

  return loop.Schedule(
      "MakeDirectory",
      Eventual<void>()
          .context(Data{loop, path, mode})
          .start([](auto& data, auto& k) mutable {
            using K = std::decay_t<decltype(k)>;

            data.k = &k;
            data.req->data = &data;

            auto error = uv_fs_mkdir(
                data.loop,
                data.req,
                data.path.string().c_str(),
                data.mode,
                [](uv_fs_t* req) {
                  auto& data = *static_cast<Data*>(req->data);
                  auto& k = *static_cast<K*>(data.k);
                  if (req->result == 0) {
                    k.Start();
                  } else {
                    k.Fail(uv_strerror(req->result));
                  }
                });

            if (error) {
              static_cast<K*>(data.k)->Fail(uv_strerror(error));
            }
          }));
}

////////////////////////////////////////////////////////////////////////

inline auto MakeDirectory(const std::filesystem::path& path, const int& mode) {
  return MakeDirectory(EventLoop::Default(), path, mode);
}

////////////////////////////////////////////////////////////////////////

inline auto RemoveDirectory(
    EventLoop& loop,
    const std::filesystem::path& path) {
  struct Data {
    EventLoop& loop;
    std::filesystem::path path;

    detail::Request req;
    void* k = nullptr;
  };

  return loop.Schedule(
      "RemoveDirectory",
      Eventual<void>()
          .context(Data{loop, path})
          .start([](auto& data, auto& k) mutable {
            using K = std::decay_t<decltype(k)>;

            data.k = &k;
            data.req->data = &data;

            auto error = uv_fs_rmdir(
                data.loop,
                data.req,
                data.path.string().c_str(),
                [](uv_fs_t* req) {
                  auto& data = *static_cast<Data*>(req->data);
                  auto& k = *static_cast<K*>(data.k);
                  if (req->result == 0) {
                    k.Start();
                  } else {
                    k.Fail(uv_strerror(req->result));
                  }
                });

            if (error) {
              static_cast<K*>(data.k)->Fail(uv_strerror(error));
            }
          }));
}

////////////////////////////////////////////////////////////////////////

inline auto RemoveDirectory(const std::filesystem::path& path) {
  return RemoveDirectory(EventLoop::Default(), path);
}

////////////////////////////////////////////////////////////////////////

inline auto CopyFile(
    EventLoop& loop,
    const std::filesystem::path& src,
    const std::filesystem::path& dst,
    const int& flags) {
  struct Data {
    EventLoop& loop;
    std::filesystem::path src;
    std::filesystem::path dst;
    int flags;

    detail::Request req;
    void* k = nullptr;
  };

  return loop.Schedule(
      "CopyFile",
      Eventual<void>()
          .context(Data{loop, src, dst, flags})
          .start([](auto& data, auto& k) mutable {
            using K = std::decay_t<decltype(k)>;

            data.k = &k;
            data.req->data = &data;

            auto error = uv_fs_copyfile(
                data.loop,
                data.req,
                data.src.string().c_str(),
                data.dst.string().c_str(),
                data.flags,
                [](uv_fs_t* req) {
                  auto& data = *static_cast<Data*>(req->data);
                  auto& k = *static_cast<K*>(data.k);
                  if (req->result == 0) {
                    k.Start();
                  } else {
                    k.Fail(uv_strerror(req->result));
                  }
                });

            if (error) {
              static_cast<K*>(data.k)->Fail(uv_strerror(error));
            }
          }));
}

////////////////////////////////////////////////////////////////////////

inline auto CopyFile(
    const std::filesystem::path& src,
    const std::filesystem::path& dst,
    const int& flags) {
  return CopyFile(EventLoop::Default(), src, dst, flags);
}

////////////////////////////////////////////////////////////////////////

inline auto RenameFile(
    EventLoop& loop,
    const std::filesystem::path& src,
    const std::filesystem::path& dst) {
  struct Data {
    EventLoop& loop;
    std::filesystem::path src;
    std::filesystem::path dst;

    detail::Request req;
    void* k = nullptr;
  };

  return loop.Schedule(
      "RenameFile",
      Eventual<void>()
          .context(Data{loop, src, dst})
          .start([](auto& data, auto& k) mutable {
            using K = std::decay_t<decltype(k)>;

            data.k = &k;
            data.req->data = &data;

            auto error = uv_fs_rename(
                data.loop,
                data.req,
                data.src.string().c_str(),
                data.dst.string().c_str(),
                [](uv_fs_t* req) {
                  auto& data = *static_cast<Data*>(req->data);
                  auto& k = *static_cast<K*>(data.k);
                  if (req->result == 0) {
                    k.Start();
                  } else {
                    k.Fail(uv_strerror(req->result));
                  }
                });

            if (error) {
              static_cast<K*>(data.k)->Fail(uv_strerror(error));
            }
          }));
}

////////////////////////////////////////////////////////////////////////

inline auto RenameFile(
    const std::filesystem::path& src,
    const std::filesystem::path& dst) {
  return RenameFile(EventLoop::Default(), src, dst);
}

////////////////////////////////////////////////////////////////////////

} // namespace filesystem
} // namespace eventuals
} // namespace stout

////////////////////////////////////////////////////////////////////////