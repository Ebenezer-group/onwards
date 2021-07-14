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

#ifndef MIO_MMAP_HEADER
#define MIO_MMAP_HEADER

#include<iterator>
#include<system_error>
#include<stdint.h>
#include<sys/mman.h>
#include<unistd.h>

namespace cmw{

/**
 * This is used to determine whether to create a read-only or
 * a read-write memory mapping.
 */
enum class access_mode
{
    read,
    write
};

namespace detail{
  
/**
 * Returns the last system error as a `std::error_code`.
 */
std::error_code last_error ()noexcept;

int open_file (::std::string_view& path, access_mode const,
	       std::error_code&);

size_t query_file_size (int handle, std::error_code&);

struct mmap_context
{
    char* data;
    int64_t length;
    int64_t mapped_length;
};

mmap_context memory_map (int const handle, int64_t const offset,
			 int64_t const length, access_mode const, std::error_code&);
} // namespace detail
  
/**
 * Determines the page allocation granularity.
 *
 * On the first call to this function, it invokes the syscall
 * to determine the page size, caches the value, and returns it. Any subsequent call to
 * this function serves the cached value, so no further syscalls are made.
 */
size_t page_size ();
  
/**
 * Alligns `offset` to the system's page size such that it subtracts the
 * difference until the nearest page boundary before `offset`, or does nothing if
 * `offset` is already page aligned.
 */
inline size_t make_offset_page_aligned (size_t offset)noexcept
{
    size_t const pageSize=page_size();
    // Use integer division to round down to the nearest page alignment.
    return offset / pageSize * pageSize;
}

// This value may be provided as the `length` parameter to the constructor or
// `map`, in which case a memory mapping of the entire file is created.
enum { map_entire_file = 0 };

// This value represents an invalid file handle type. This can be used to
// determine whether `basic_mmap::file_handle` is valid, for example.
static int const invalid_handle = -1;

template<access_mode AccessMode, typename ByteT>
struct basic_mmap
{
    using value_type = ByteT;
    using size_type = size_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using difference_type = std::ptrdiff_t;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using iterator_category = std::random_access_iterator_tag;
    using handle_type = int;

    static_assert(sizeof(ByteT)==sizeof(char), "ByteT must be the same size as char.");

private:
    // Points to the first requested byte, and not to the actual start of the mapping.
    pointer data_ = nullptr;

    // Length--in bytes--requested by user (which may not be the length of the
    // full mapping) and the length of the full mapping.
    size_type length_ = 0;
    size_type mapped_length_ = 0;

    // Letting user map a file using both an existing file handle and a path
    // introcudes some complexity (see `is_handle_internal_`).
    // On POSIX, we only need a file handle to create a mapping, while on
    // Windows systems the file handle is necessary to retrieve a file mapping
    // handle, but any subsequent operations on the mapped region must be done
    // through the latter.
    handle_type file_handle_ = invalid_handle;

    // Letting user map a file using both an existing file handle and a path
    // introcudes some complexity in that we must not close the file handle if
    // user provided it, but we must close it if we obtained it using the
    // provided path. For this reason, this flag is used to determine when to
    // close `file_handle_`.
    bool is_handle_internal_;

public:
    /**
     * The default constructed mmap object is in a non-mapped state, that is,
     * any operation that attempts to access nonexistent underlying data will
     * result in undefined behaviour/segmentation faults.
     */
    basic_mmap ()=default;

#ifdef __cpp_exceptions
    /**
     * The same as invoking the `map` function, except any error that may occur
     * while establishing the mapping is wrapped in a `std::system_error` and is
     * thrown.
     */
    basic_mmap (std::string_view const& path, size_type const offset = 0, size_type const length = map_entire_file)
    {
        std::error_code error;
        map(path, offset, length, error);
        if(error) { throw std::system_error(error); }
    }

    /**
     * The same as invoking the `map` function, except any error that may occur
     * while establishing the mapping is wrapped in a `std::system_error` and is
     * thrown.
     */
    basic_mmap (handle_type const handle, size_type const offset = 0, size_type const length = map_entire_file)
    {
        std::error_code error;
        map(handle, offset, length, error);
        if(error) { throw std::system_error(error); }
    }
#endif // __cpp_exceptions

    /**
     * `basic_mmap` has single-ownership semantics, so transferring ownership
     * may only be accomplished by moving the object.
     */
    basic_mmap (const basic_mmap&) = delete;
    basic_mmap (basic_mmap&&);
    basic_mmap& operator= (const basic_mmap&) = delete;
    basic_mmap& operator= (basic_mmap&&);

    /** Flushes the memory mapped page to disk. Errors are reported via `error`. */
    template<access_mode A = AccessMode>
    typename std::enable_if<A == access_mode::write, void>::type
    sync (std::error_code& error);

    /**
     * If this is a read-write mapping, the destructor invokes sync. Regardless
     * of the access mode, unmap is invoked as a final step.
     */
    ~basic_mmap (){unmap();}

    handle_type file_handle () const { return file_handle_; }

    /** Returns whether a valid memory mapping has been created. */
    bool is_open ()const { return file_handle_ != invalid_handle; }

    /**
     * Returns true if no mapping was established, that is, conceptually the
     * same as though the length that was mapped was 0. This function is
     * provided so that this class has Container semantics.
     */
    bool empty ()const noexcept { return length() == 0; }

    /** Returns true if a mapping was established. */
    bool is_mapped ()const noexcept;

    /**
     * `length` returns the logical length, i.e. the number of bytes
     * user requested to be mapped, while `mapped_length` returns the actual number of
     * bytes that were mapped which is a multiple of the underlying operating system's
     * page allocation granularity.
     */
    size_type length ()const { return length_; }
    size_type mapped_length ()const { return mapped_length_; }

    /** Returns the offset relative to the start of the mapping. */
    size_type mapping_offset ()const 
    {
        return mapped_length_ - length_;
    }

    /**
     * Returns a pointer to the first requested byte, or `nullptr` if no memory mapping
     * exists.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > pointer data ()noexcept { return data_; }
    const_pointer data ()const noexcept { return data_; }

    /**
     * Returns an iterator to the first requested byte, if a valid memory mapping
     * exists, otherwise this function call is undefined behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > iterator begin (){ return data(); }
    const_iterator begin ()const { return data(); }
    const_iterator cbegin ()const { return data(); }

    /**
     * Returns an iterator one past the last requested byte, if a valid memory mapping
     * exists, otherwise this function call is undefined behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > iterator end () { return data() + length(); }
    const_iterator end ()const { return data() + length(); }
    const_iterator cend ()const { return data() + length(); }

    /**
     * Returns a reverse iterator to the last memory mapped byte, if a valid
     * memory mapping exists, otherwise this function call is undefined
     * behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > reverse_iterator rbegin (){ return reverse_iterator(end()); }
    const_reverse_iterator rbegin ()const
    { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin ()const
    { return const_reverse_iterator(end()); }

    /**
     * Returns a reverse iterator past the first mapped byte, if a valid memory
     * mapping exists, otherwise this function call is undefined behaviour.
     */
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > reverse_iterator rend (){ return reverse_iterator(begin()); }
    const_reverse_iterator rend ()const
    { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend ()const
    { return const_reverse_iterator(begin()); }

    /**
     * Returns a reference to the `i`th byte from the first requested byte (as returned
     * by `data`). If this is invoked when no valid memory mapping has been created
     * prior to this call, undefined behaviour ensues.
     */
    reference operator[] (size_type const i){ return data_[i]; }
    const_reference operator[] (size_type const i)const { return data_[i]; }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `path`, which must be a path to an existing file, is used to retrieve a file
     * handle (which is closed when the object destructs or `unmap` is called), which is
     * then used to memory map the requested region. Upon failure, `error` is set to
     * indicate the reason and the object remains in an unmapped state.
     *
     * `offset` is the number of bytes, relative to the start of the file, where the
     * mapping should begin. When specifying it, there is no need to worry about
     * providing a value that is aligned with the operating system's page allocation
     * granularity. This is adjusted by the implementation such that the first requested
     * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
     * `offset` from the start of the file.
     *
     * `length` is the number of bytes to map. It may be `map_entire_file`, in which
     * case a mapping of the entire file is created.
     */
  void map (::std::string_view path, size_type const offset,
            size_type const length, std::error_code& error);

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `path`, which must be a path to an existing file, is used to retrieve a file
     * handle (which is closed when the object destructs or `unmap` is called), which is
     * then used to memory map the requested region. Upon failure, `error` is set to
     * indicate the reason and the object remains in an unmapped state.
     * 
     * The entire file is mapped.
     */
  void map (::std::string_view path, std::error_code& error)
    {
        map(path, 0, map_entire_file, error);
    }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is
     * unsuccesful, the reason is reported via `error` and the object remains in
     * a state as if this function hadn't been called.
     *
     * `handle`, which must be a valid file handle, which is used to memory map the
     * requested region. Upon failure, `error` is set to indicate the reason and the
     * object remains in an unmapped state.
     *
     * `offset` is the number of bytes, relative to the start of the file, where the
     * mapping should begin. When specifying it, there is no need to worry about
     * providing a value that is aligned with the operating system's page allocation
     * granularity. This is adjusted by the implementation such that the first requested
     * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
     * `offset` from the start of the file.
     *
     * `length` is the number of bytes to map. It may be `map_entire_file`, in which
     * case a mapping of the entire file is created.
     */
    void map (handle_type const handle, size_type const offset,
            size_type const length, std::error_code& error);

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is
     * unsuccesful, the reason is reported via `error` and the object remains in
     * a state as if this function hadn't been called.
     *
     * `handle`, which must be a valid file handle, which is used to memory map the
     * requested region. Upon failure, `error` is set to indicate the reason and the
     * object remains in an unmapped state.
     * 
     * The entire file is mapped.
     */
    void map (handle_type const handle, std::error_code& error)
    {
        map(handle, 0, map_entire_file, error);
    }

    /**
     * If a valid memory mapping has been created prior to this call, this call
     * instructs the kernel to unmap the memory region and disassociate this object
     * from the file.
     *
     * The file handle associated with the file that is mapped is only closed if the
     * mapping was created using a file path. If, on the other hand, an existing
     * file handle was used to create the mapping, the file handle is not closed.
     */
    void unmap ();

    void swap (basic_mmap&);

    /**
     * All operators compare the address of the first byte and size of the two mapped
     * regions.
     */

private:
    template<
        access_mode A = AccessMode,
        typename = typename std::enable_if<A == access_mode::write>::type
    > pointer get_mapping_start ()noexcept
    {
        return !data() ? nullptr : data() - mapping_offset();
    }

    const_pointer get_mapping_start ()const noexcept
    {
        return !data() ? nullptr : data() - mapping_offset();
    }
};

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
basic_mmap<AccessMode, ByteT>::basic_mmap (basic_mmap&& other)
    : data_(other.data_)
    , length_(other.length_)
    , mapped_length_(other.mapped_length_)
    , file_handle_(other.file_handle_)
    , is_handle_internal_(other.is_handle_internal_)
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
        data_ = other.data_;
        length_ = other.length_;
        mapped_length_ = other.mapped_length_;
        file_handle_ = other.file_handle_;
        is_handle_internal_ = other.is_handle_internal_;

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
void basic_mmap<AccessMode, ByteT>::map (handle_type const handle,
        size_type const offset, size_type const length, std::error_code& error)
{
    error.clear();
    if(handle == invalid_handle)
    {
        error = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    auto const file_size = detail::query_file_size(handle, error);
    if(error)
    {
        return;
    }

    if(offset + length > file_size)
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    auto const ctx = detail::memory_map(handle, offset,
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
void basic_mmap<AccessMode, ByteT>::map (::std::string_view path, size_type const offset,
        size_type const length, std::error_code& error)
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

/**
 * This is the basis for all read-only mmap objects and should be preferred over
 * directly using `basic_mmap`.
 */
template<typename ByteT>
using basic_mmap_source = basic_mmap<access_mode::read, ByteT>;

/**
 * This is the basis for all read-write mmap objects and should be preferred over
 * directly using `basic_mmap`.
 */
template<typename ByteT>
using basic_mmap_sink = basic_mmap<access_mode::write, ByteT>;

/**
 * These aliases cover the most common use cases, both representing a raw byte stream
 * (either with a char or an unsigned char/uint8_t).
 */
using mmap_source = basic_mmap_source<char>;
using ummap_source = basic_mmap_source<unsigned char>;

using mmap_sink = basic_mmap_sink<char>;
using ummap_sink = basic_mmap_sink<unsigned char>;

/**
 * Convenience factory method that constructs a mapping for any `basic_mmap` or
 * `basic_mmap` type.
 */
template<
    typename MMap,
    typename MappingToken
> MMap make_mmap(MappingToken const& token,
        int64_t offset, int64_t length, std::error_code& error)
{
    MMap mmap;
    mmap.map(token, offset, length, error);
    return mmap;
}

/**
 * Convenience factory method.
 *
 * MappingToken may be a String (`std::string`, `std::string_view`, `const char*`,
 * `std::filesystem::path`, `std::vector<char>`, or similar), or a
 * `mmap_source::handle_type`.
 */
template<typename MappingToken>
mmap_source make_mmap_source (MappingToken const& token, mmap_source::size_type offset,
        mmap_source::size_type length, std::error_code& error)
{
    return make_mmap<mmap_source>(token, offset, length, error);
}

template<typename MappingToken>
mmap_source make_mmap_source (MappingToken const& token, std::error_code& error)
{
    return make_mmap_source(token, 0, map_entire_file, error);
}

/**
 * Convenience factory method.
 */
template<typename MappingToken>
mmap_sink make_mmap_sink (MappingToken const& token, mmap_sink::size_type offset,
        mmap_sink::size_type length, std::error_code& error)
{
    return make_mmap<mmap_sink>(token, offset, length, error);
}

template<typename MappingToken>
mmap_sink make_mmap_sink (MappingToken const& token, std::error_code& error)
{
    return make_mmap_sink(token, 0, map_entire_file, error);
}

} // namespace cmw
#endif // MIO_MMAP_HEADER
