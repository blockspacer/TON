#include "td/utils/port/MemoryMapping.h"

#include "td/utils/misc.h"

// TODO:
// windows,
// anonymous map
// huge pages?
#if TD_WINDOWS
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace td {
namespace detail {
class MemoryMappingImpl {
 public:
  MemoryMappingImpl(MutableSlice data, int64 offset) : data_(data), offset_(offset) {
  }
  Slice as_slice() const {
    return data_.substr(narrow_cast<size_t>(offset_));
  }
  MutableSlice as_mutable_slice() const {
    return {};
  }

 private:
  MutableSlice data_;
  int64 offset_;
};

Result<int64> get_page_size() {
#if TD_WINDOWS
  return Status::Error("Unimplemented");
#else
  static Result<int64> page_size = []() -> Result<int64> {
    auto page_size = sysconf(_SC_PAGESIZE);
    if (page_size < 0) {
      return OS_ERROR("Can't load page size from sysconf");
    }
    return page_size;
  }();
  return page_size.clone();
#endif
}
}  // namespace detail

Result<MemoryMapping> MemoryMapping::create_anonymous(const MemoryMapping::Options &options) {
  return Status::Error("Unsupported yet");
}
Result<MemoryMapping> MemoryMapping::create_from_file(const FileFd &file_fd, const MemoryMapping::Options &options) {
#if TD_WINDOWS
  return Status::Error("Unsupported yet");
#else
  if (file_fd.empty()) {
    return Status::Error("Can't create memory mapping: file is empty");
  }
  TRY_RESULT(stat, file_fd.stat());
  auto fd = file_fd.get_native_fd().fd();
  auto begin = options.offset;
  if (begin < 0) {
    return Status::Error(PSLICE() << "Can't create memory mapping: negative offset " << options.offset);
  }

  int64 end;
  if (options.size < 0) {
    end = stat.size_;
  } else {
    end = begin + stat.size_;
  }

  TRY_RESULT(page_size, detail::get_page_size());
  auto fixed_begin = begin / page_size * page_size;

  auto data_offset = begin - fixed_begin;
  auto data_size = end - fixed_begin;

  void *data = mmap(nullptr, data_size, PROT_READ, MAP_PRIVATE, fd, fixed_begin);
  if (data == MAP_FAILED) {
    return OS_ERROR("mmap call failed");
  }

  return MemoryMapping(std::make_unique<detail::MemoryMappingImpl>(
      MutableSlice(reinterpret_cast<char *>(data), data_size), data_offset));
#endif
}

MemoryMapping::MemoryMapping(MemoryMapping &&other) = default;
MemoryMapping &MemoryMapping::operator=(MemoryMapping &&other) = default;
MemoryMapping::~MemoryMapping() = default;

MemoryMapping::MemoryMapping(std::unique_ptr<detail::MemoryMappingImpl> impl) : impl_(std::move(impl)) {
}
Slice MemoryMapping::as_slice() const {
  return impl_->as_slice();
}
MutableSlice MemoryMapping::as_mutable_slice() {
  return impl_->as_mutable_slice();
}

}  // namespace td

