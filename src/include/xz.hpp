// (C) Copyright 2008 CodeRage, LLC (turkanis at coderage dot com)
// (C) Copyright 2003-2007 Jonathan Turkanis
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.)

// See http://www.boost.org/libs/iostreams for documentation.

// Note: custom allocators are not supported on VC6, since that compiler
// had trouble finding the function zlib_base::do_init.

#ifndef BOOST_IOSTREAMS_XZ_HPP_INCLUDED
#define BOOST_IOSTREAMS_XZ_HPP_INCLUDED

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <lzma.h>
#include <cassert>
#include <memory>            // allocator.
#include <new>               // bad_alloc.
#include <boost/config.hpp>  // MSVC, STATIC_CONSTANT, DEDUCED_TYPENAME, DINKUM.
#include <boost/detail/workaround.hpp>
#include <boost/iostreams/constants.hpp>   // buffer size.
#include <boost/iostreams/detail/config/auto_link.hpp>
#include <boost/iostreams/detail/config/dyn_link.hpp>
#include <boost/iostreams/detail/config/wide_streams.hpp>
#include <boost/iostreams/detail/ios.hpp>  // failure, streamsize.
#include <boost/iostreams/filter/symmetric.hpp>               
#include <boost/iostreams/pipeline.hpp>       
#include <boost/type_traits/is_same.hpp>     

// Must come last.
#ifdef BOOST_MSVC
# pragma warning(push)
# pragma warning(disable:4251 4231 4660)
#endif
#include <boost/config/abi_prefix.hpp>           

// Temporary fix.
#undef small

namespace boost { namespace iostreams {

namespace xz {

                    // Typedefs.

typedef void* (*alloc_func)(void*, int, int);
typedef void (*free_func)(void*, void*);

                    // Status codes

BOOST_IOSTREAMS_DECL extern const int ok;
BOOST_IOSTREAMS_DECL extern const int stream_end;    
BOOST_IOSTREAMS_DECL extern const int param_error;
BOOST_IOSTREAMS_DECL extern const int mem_error;
BOOST_IOSTREAMS_DECL extern const int data_error;
BOOST_IOSTREAMS_DECL extern const int data_error_magic;

                    // Action codes

BOOST_IOSTREAMS_DECL extern const int finish;
BOOST_IOSTREAMS_DECL extern const int run;

                    // Default values

const int default_preset   = 6;


} // End namespace xz.

//
// Class name: xz_params.
// Description: Encapsulates the parameters passed to deflateInit2
//      to customize compression.
//


//
// Class name: xz_error.
// Description: Subclass of std::ios_base::failure thrown to indicate
//     xz errors other than out-of-memory conditions.
//
class BOOST_IOSTREAMS_DECL xz_error : public BOOST_IOSTREAMS_FAILURE {
public:
    explicit xz_error(int error);
    int error() const { return error_; }
    static void check(int error);
private:
    int error_;
};

namespace detail {

template<typename Alloc>
struct xz_allocator_traits {
#ifndef BOOST_NO_STD_ALLOCATOR
    typedef typename Alloc::template rebind<char>::other type;
#else
    typedef std::allocator<char> type;
#endif
};

template< typename Alloc,
          typename Base = // VC6 workaround (C2516)
              BOOST_DEDUCED_TYPENAME xz_allocator_traits<Alloc>::type >
struct xz_allocator : private Base {
private:
    typedef typename Base::size_type size_type;
public:
    BOOST_STATIC_CONSTANT(bool, custom = 
        (!is_same<std::allocator<char>, Base>::value));
    typedef typename xz_allocator_traits<Alloc>::type allocator_type;
    static void* allocate(void* self, int items, int size);
    static void deallocate(void* self, void* address);
};

class BOOST_IOSTREAMS_DECL xz_base  {
public:
    typedef char char_type;
protected:
    xz_base(const uint32_t preset=xz::default_preset);
    ~xz_base();
    bool& ready() { return ready_; }
    template<typename Alloc> 
    void init( bool compress,
               xz_allocator<Alloc>& alloc )
        {
            bool custom = xz_allocator<Alloc>::custom;
            std::cerr << custom << std::endl;
            do_init( compress,
                     #if !BOOST_WORKAROUND(BOOST_MSVC, < 1300)
                         custom ? xz_allocator<Alloc>::allocate : 0,
                         custom ? xz_allocator<Alloc>::deallocate : 0,
                     #endif
                     custom ? &alloc : 0 );
        }
    void before( const char*& src_begin, const char* src_end,
                 char*& dest_begin, char* dest_end );
    void after(const char*& src_begin, char*& dest_begin);
    int compress(int action);
    int decompress();
    void end(bool compress);
private:
    void do_init( bool compress, 
                  #if !BOOST_WORKAROUND(BOOST_MSVC, < 1300)
                      xz::alloc_func,
                      xz::free_func,
                  #endif
                  void* derived );
    uint32_t      preset_; // Preset used for compression
    void*         stream_; // Actual type: lzma_stream*.
    bool          ready_;
};

//
// Template name: xz_compressor_impl
// Description: Model of SymmetricFilter implementing compression by
//      delegating to the liblzma functions lzma_easy_encoder and lzma_code().
//
template<typename Alloc = std::allocator<char> >
class xz_compressor_impl
    : public xz_base,
      #if BOOST_WORKAROUND(__BORLANDC__, < 0x600)
          public
      #endif
      xz_allocator<Alloc>
{
public: 
    xz_compressor_impl(uint32_t xz_preset);
    bool filter( const char*& src_begin, const char* src_end,
                 char*& dest_begin, char* dest_end, bool flush );
    void close();
private:
    void init();
};

//
// Template name: xz_compressor
// Description: Model of SymmetricFilter implementing decompression by
//      delegating to the liblzma function lzma_easy_encoder and lzma_code().
//
template<typename Alloc = std::allocator<char> >
class xz_decompressor_impl
    : public xz_base,
      #if BOOST_WORKAROUND(__BORLANDC__, < 0x600)
          public
      #endif
      xz_allocator<Alloc>
{ 
public:
    xz_decompressor_impl();
    bool filter( const char*& begin_in, const char* end_in,
                 char*& begin_out, char* end_out, bool flush );
    void close();
private:
    void init();
    bool eof_; // Guard to make sure filter() isn't called after it returns false.
};

} // End namespace detail.

//
// Template name: xz_compressor
// Description: Model of InputFilter and OutputFilter implementing
//      compression using libxz.
//
template<typename Alloc = std::allocator<char> >
struct basic_xz_compressor
    : symmetric_filter<detail::xz_compressor_impl<Alloc>, Alloc>
{
private:
    typedef detail::xz_compressor_impl<Alloc>   impl_type;
    typedef symmetric_filter<impl_type, Alloc>  base_type;
public:
    typedef typename base_type::char_type               char_type;
    typedef typename base_type::category                category;
    basic_xz_compressor(uint32_t = xz::default_preset, int buffer_size =  default_device_buffer_size );
};
BOOST_IOSTREAMS_PIPABLE(basic_xz_compressor, 1)

typedef basic_xz_compressor<> xz_compressor;

//
// Template name: xz_decompressor
// Description: Model of InputFilter and OutputFilter implementing
//      decompression using libxz.
//
template<typename Alloc = std::allocator<char> >
struct basic_xz_decompressor
    : symmetric_filter<detail::xz_decompressor_impl<Alloc>, Alloc>
{
private:
    typedef detail::xz_decompressor_impl<Alloc>      impl_type;
    typedef symmetric_filter<impl_type, Alloc>  base_type;
public:
    typedef typename base_type::char_type               char_type;
    typedef typename base_type::category                category;
    basic_xz_decompressor(int buffer_size = default_device_buffer_size );
};
BOOST_IOSTREAMS_PIPABLE(basic_xz_decompressor, 1)

typedef basic_xz_decompressor<> xz_decompressor;

//----------------------------------------------------------------------------//

//------------------Implementation of xz_allocator-------------------------//

namespace detail {

template<typename Alloc, typename Base>
void* xz_allocator<Alloc, Base>::allocate(void* self, int items, int size)
{ 
    size_type len = items * size;
    char* ptr = 
        static_cast<allocator_type*>(self)->allocate
            (len + sizeof(size_type)
            #if BOOST_WORKAROUND(BOOST_DINKUMWARE_STDLIB, == 1)
                , (char*)0
            #endif
            );
    *reinterpret_cast<size_type*>(ptr) = len;
    return ptr + sizeof(size_type);
}

template<typename Alloc, typename Base>
void xz_allocator<Alloc, Base>::deallocate(void* self, void* address)
{ 
    char* ptr = reinterpret_cast<char*>(address) - sizeof(size_type);
    size_type len = *reinterpret_cast<size_type*>(ptr) + sizeof(size_type);
    static_cast<allocator_type*>(self)->deallocate(ptr, len); 
}

//------------------Implementation of xz_compressor_impl-------------------//

template<typename Alloc>
xz_compressor_impl<Alloc>::xz_compressor_impl(uint32_t p)
    : xz_base(p) { }

template<typename Alloc>
bool xz_compressor_impl<Alloc>::filter
    ( const char*& src_begin, const char* src_end,
      char*& dest_begin, char* dest_end, bool flush )
{
    if (!ready()) init();
    before(src_begin, src_end, dest_begin, dest_end);
    int result = compress(flush ? xz::finish : xz::run);
    after(src_begin, dest_begin);
    xz_error::check(result);
    return result != xz::stream_end;
}

template<typename Alloc>
void xz_compressor_impl<Alloc>::close()
{ 
    end(true); 
}

template<typename Alloc>
inline void xz_compressor_impl<Alloc>::init()
{ xz_base::init(true, static_cast<xz_allocator<Alloc>&>(*this)); }

//------------------Implementation of xz_decompressor_impl-----------------//

template<typename Alloc>
xz_decompressor_impl<Alloc>::xz_decompressor_impl() : xz_base(), eof_(false) { }

template<typename Alloc>
bool xz_decompressor_impl<Alloc>::filter
    ( const char*& src_begin, const char* src_end,
      char*& dest_begin, char* dest_end, bool /* flush */ )
{
    if (!ready()) 
        init();
    if (eof_) 
        return false;
    before(src_begin, src_end, dest_begin, dest_end);
    int result = decompress();
    after(src_begin, dest_begin);
    xz_error::check(result);
    return !(eof_ = result == xz::stream_end);
}

template<typename Alloc>
void xz_decompressor_impl<Alloc>::close()
{ 
    try {
        end(false);
    } catch (...) { 
        eof_ = false; 
        throw;
    }
    eof_ = false;
}

template<typename Alloc>
inline void xz_decompressor_impl<Alloc>::init()
{ xz_base::init(false, static_cast<xz_allocator<Alloc>&>(*this)); }
} // End namespace detail.

//------------------Implementation of xz_compressor----------------------//

template<typename Alloc>
basic_xz_compressor<Alloc>::basic_xz_compressor
        (uint32_t p, int buffer_size)
    : base_type(buffer_size, p) 
    { }

//------------------Implementation of xz_decompressor----------------------//

template<typename Alloc>
basic_xz_decompressor<Alloc>::basic_xz_decompressor
        (int buffer_size)
    : base_type(buffer_size)
    { }

//----------------------------------------------------------------------------//

} } // End namespaces iostreams, boost.

#include <boost/config/abi_suffix.hpp> // Pops abi_suffix.hpp pragmas.
#ifdef BOOST_MSVC
# pragma warning(pop)
#endif

#endif // #ifndef BOOST_IOSTREAMS_XZ_HPP_INCLUDED
