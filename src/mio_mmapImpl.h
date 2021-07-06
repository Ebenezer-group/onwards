/* Copyright 2017 https://github.com/mandreyel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 */

#ifndef MIO_BASIC_MMAP_IMPL
#define MIO_BASIC_MMAP_IMPL

#include"mio_mmap.h"

#include<fcntl.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<unistd.h>

namespace mio{
namespace detail{
  
/**
 * Returns the last system error as a `std::error_code`.
 */
std::error_code last_error ()noexcept
{
    std::error_code error(errno, std::system_category());
    return error;
}

int open_file (::std::string_view& path, access_mode const mode,
        std::error_code& error)
{
    error.clear();
    if(path.empty())
    {
        error=std::make_error_code(std::errc::invalid_argument);
        return invalid_handle;
    }
    auto const handle=::open(path.data(), mode==access_mode::read?O_RDONLY:O_RDWR);
    if(handle==invalid_handle)
    {
        error=detail::last_error();
    }
    return handle;
}

size_t query_file_size (int handle, std::error_code& error)
{
    error.clear();
    struct stat sbuf;
    if(::fstat(handle, &sbuf)==-1)
    {
        error=detail::last_error();
        return 0;
    }
    return sbuf.st_size;
}

struct mmap_context
{
    char* data;
    int64_t length;
    int64_t mapped_length;
};

mmap_context memory_map (int const file_handle, int64_t const offset,
    int64_t const length, access_mode const mode, std::error_code& error)
{
    int64_t const alignedOffset = make_offset_page_aligned(offset);
    int64_t const length_to_map = offset - alignedOffset + length;
    char* mapping_start=static_cast<char*>(::mmap(
            0, // Don't give hint as to where to map.
            length_to_map,
            mode==access_mode::read?PROT_READ:PROT_WRITE,
            MAP_SHARED,
            file_handle,
            alignedOffset));
    if(mapping_start == MAP_FAILED)
    {
        error=detail::last_error();
        return {};
    }
    mmap_context ctx;
    ctx.data = mapping_start + offset - alignedOffset;
    ctx.length = length;
    ctx.mapped_length = length_to_map;
    return ctx;
}
} // namespace detail

size_t page_size ()
{
    static size_t const pageSize = []
    {
      return ::sysconf(_SC_PAGE_SIZE);
    }();
    return pageSize;
}

// basic_mmap
  
template<access_mode AccessMode, typename ByteT>
template<access_mode A>
typename std::enable_if<A == access_mode::write, void>::type
basic_mmap<AccessMode, ByteT>::sync (std::error_code& error)
{
    error.clear();
    if(!is_open())
    {
        error=std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    if(data())
    {
        if(::msync(get_mapping_start(), mapped_length_, MS_SYNC) != 0)
        {
            error=detail::last_error();
            return;
        }
    }
}

template<access_mode AccessMode, typename ByteT>
basic_mmap<AccessMode, ByteT>::~basic_mmap ()
{
    if constexpr(AccessMode==access_mode::write){
      std::error_code ec;
      sync(ec);
    }
    unmap();
}

template<access_mode AccessMode, typename ByteT>
basic_mmap<AccessMode, ByteT>::basic_mmap (basic_mmap&& other)
    : data_(std::move(other.data_))
    , length_(std::move(other.length_))
    , mapped_length_(std::move(other.mapped_length_))
    , file_handle_(std::move(other.file_handle_))
    , is_handle_internal_(std::move(other.is_handle_internal_))
{
    other.data_ = nullptr;
    other.length_ = other.mapped_length_ = 0;
    other.file_handle_ = invalid_handle;
}

template<access_mode AccessMode, typename ByteT>
basic_mmap<AccessMode, ByteT>&
basic_mmap<AccessMode, ByteT>::operator= (basic_mmap&& other)
{
    if(this != &other)
    {
        // First the existing mapping needs to be removed.
        unmap();
        data_ = std::move(other.data_);
        length_ = std::move(other.length_);
        mapped_length_ = std::move(other.mapped_length_);
        file_handle_ = std::move(other.file_handle_);
        is_handle_internal_ = std::move(other.is_handle_internal_);

        // The moved from basic_mmap's fields need to be reset, because
        // otherwise other's destructor will unmap the same mapping that was
        // just moved into this.
        other.data_ = nullptr;
        other.length_ = other.mapped_length_ = 0;
        other.file_handle_ = invalid_handle;
        other.is_handle_internal_ = false;
    }
    return *this;
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::map (::std::string_view path, const size_type offset,
        const size_type length, std::error_code& error)
{
    error.clear();
    if(path.empty())
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    auto const handle=detail::open_file(path, AccessMode, error);
    if(error)
    {
        return;
    }

    map(handle, offset, length, error);
    // This MUST be after the call to map, as that sets this to true.
    if(!error)
    {
        is_handle_internal_ = true;
    }
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::map (const handle_type handle,
        const size_type offset, const size_type length, std::error_code& error)
{
    error.clear();
    if(handle == invalid_handle)
    {
        error = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    const auto file_size = detail::query_file_size(handle, error);
    if(error)
    {
        return;
    }

    if(offset + length > file_size)
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    const auto ctx = detail::memory_map(handle, offset,
            length == map_entire_file ? (file_size - offset) : length,
            AccessMode, error);
    if(!error)
    {
        // We must unmap the previous mapping that may have existed prior to this call.
        // Note that this must only be invoked after a new mapping has been created in
        // order to provide the strong guarantee that, should the new mapping fail, the
        // `map` function leaves this instance in a state as though the function had
        // never been invoked.
        unmap();
        file_handle_ = handle;
        is_handle_internal_ = false;
        data_ = reinterpret_cast<pointer>(ctx.data);
        length_ = ctx.length;
        mapped_length_ = ctx.mapped_length;
    }
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::unmap ()
{
    if(!is_open()) { return; }
    // TODO do we care about errors here?
    if(data_) { ::munmap(const_cast<pointer>(get_mapping_start()), mapped_length_); }

    // If `file_handle_` was obtained by our opening it (when map is called with
    // a path, rather than an existing file handle), we need to close it,
    // otherwise it must not be closed as it may still be used outside this
    // instance.
    if(is_handle_internal_)
    {
        ::close(file_handle_);
    }

    // Reset fields to their default values.
    data_ = nullptr;
    length_ = mapped_length_ = 0;
    file_handle_ = invalid_handle;
}

template<access_mode AccessMode, typename ByteT>
bool basic_mmap<AccessMode, ByteT>::is_mapped ()const noexcept
{
    return is_open();
}

template<access_mode AccessMode, typename ByteT>
void basic_mmap<AccessMode, ByteT>::swap (basic_mmap& other)
{
    if(this != &other)
    {
        using std::swap;
        swap(data_, other.data_);
        swap(file_handle_, other.file_handle_);
        swap(length_, other.length_);
        swap(mapped_length_, other.mapped_length_);
        swap(is_handle_internal_, other.is_handle_internal_);
    }
}

template<access_mode AccessMode, typename ByteT>
bool operator== (basic_mmap<AccessMode, ByteT> const& a,
        basic_mmap<AccessMode, ByteT> const& b)
{
    return a.data() == b.data()
        && a.size() == b.size();
}

template<access_mode AccessMode, typename ByteT>
bool operator!=(basic_mmap<AccessMode, ByteT> const& a,
        basic_mmap<AccessMode, ByteT> const& b)
{
    return !(a == b);
}

template<access_mode AccessMode, typename ByteT>
bool operator< (basic_mmap<AccessMode, ByteT> const& a,
        basic_mmap<AccessMode, ByteT> const& b)
{
    if(a.data() == b.data()) { return a.size() < b.size(); }
    return a.data() < b.data();
}

template<access_mode AccessMode, typename ByteT>
bool operator<= (basic_mmap<AccessMode, ByteT> const& a,
        basic_mmap<AccessMode, ByteT> const& b)
{
    return !(a > b);
}

template<access_mode AccessMode, typename ByteT>
bool operator> (basic_mmap<AccessMode, ByteT> const& a,
        basic_mmap<AccessMode, ByteT> const& b)
{
    if(a.data() == b.data()) { return a.size() > b.size(); }
    return a.data() > b.data();
}

template<access_mode AccessMode, typename ByteT>
bool operator>= (basic_mmap<AccessMode, ByteT> const& a,
        basic_mmap<AccessMode, ByteT> const& b)
{
    return !(a < b);
}

} // namespace mio
#endif // MIO_BASIC_MMAP_IMPL
