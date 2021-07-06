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
#include<sys/stat.h>

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

} // namespace mio
#endif // MIO_BASIC_MMAP_IMPL
