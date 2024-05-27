#pragma once
#include <memory>
#include "MyCpp/Win32Base.hpp"

namespace mycpp
{
	template < typename T >
	struct local_memory_deleter
	{
		typedef T* pointer;
		void operator () ( T* p )
		{
			if ( p != null )
				::LocalFree( reinterpret_cast< HLOCAL >( p ) );
		}
	};

	template < typename T >
	struct global_memory_deleter
	{
		typedef T* pointer;
		void operator () ( T* p )
		{
			if ( p != null )
				::GlobalFree( reinterpret_cast< HGLOBAL >( p ) );
		}
	};

	template < typename T >
	struct virtual_memory_deleter
	{
		typedef T* pointer;
		void operator () ( T* p )
		{
			if ( p != null )
				::VirtualFree( p, 0, MEM_RELEASE );
		}
	};

	typedef HANDLE heaphadle_t;

	namespace Details
	{
		struct heapmem_header
		{
			heaphadle_t hHeap;
			byte data[1];
		};

		inline heapmem_header* GetHeapMemoryHeader( void* ptr )
		{
			byte* p = reinterpret_cast< byte* >( ptr );
			heapmem_header* hdr = reinterpret_cast< heapmem_header* >( p - offsetof( heapmem_header, data ) );
			return hdr;
		}
	}

	template < typename T >
	struct heap_memory_deleter
	{
		typedef T* pointer;
		void operator () ( T* p )
		{
			if ( p != null )
			{
				Details::heapmem_header* hdr = Details::GetHeapMemoryHeader( p );
				heaphadle_t hHeap = hdr->hHeap;

				::HeapFree( hHeap, 0, hdr );

				if ( hHeap != null && hHeap != ::GetProcessHeap() )
					::HeapDestroy( hdr->hHeap );
			}
		}
	};

	template < typename T, typename AllocFunc, typename ... Args >
	inline T* malloc_func_adapter( AllocFunc allocFunc, Args&& ... args )
	{
		T* ptr = static_cast< T* >( reinterpret_cast< void* >( allocFunc( std::forward< Args >( args ) ... ) ) );

		if ( ptr == nullptr )
			throw std::bad_alloc();

		return ptr;
	}

	template < typename T >
	inline T* lcallocate( uint flags, std::size_t size )
	{
		return malloc_func_adapter< T >( &::LocalAlloc, flags, size );
	}

	template < typename T >
	inline T* glallocate( uint flags, std::size_t size )
	{
		return malloc_func_adapter< T >( &::GlobalAlloc, flags, size );
	}

	template < typename T >
	inline T* vtallocate( std::size_t size, dword allocationType, dword flagProtect, void* startAddr = null )
	{
		return malloc_func_adapter< T >( &::VirtualAlloc, startAddr, size, allocationType, flagProtect );
	}

	template < typename T >
	inline T* hpallocate( dword flags, std::size_t size, heaphadle_t hheap = ::GetProcessHeap() )
	{
		Details::heapmem_header* hdr = malloc_func_adapter< Details::heapmem_header* >( &::HeapAlloc, hheap, flags, sizeof( heapmem_header ) + size );
		hdr->hHeap = hheap;
		return reinterpret_cast< T* >( hdr->data );
	}

	template < typename T, typename Deleter >
	using scoped_memory_t = std::unique_ptr< T, Deleter >;

	template < typename T >
	using shared_memory_t = std::shared_ptr< T >;

	template < typename T > using scoped_local_memory = scoped_memory_t< T, local_memory_deleter< T > >;
	template < typename T > using shared_local_memory = shared_memory_t< T >;

	template < typename T > using scoped_global_memory = scoped_memory_t< T, global_memory_deleter< T > >;
	template < typename T > using shared_global_memory = shared_memory_t< T >;

	template < typename T > using scoped_virtual_memory = scoped_memory_t< T, virtual_memory_deleter< T > >;
	template < typename T > using shared_virtual_memory = shared_memory_t< T >;

	template < typename T > using scoped_heap_memory = scoped_memory_t< T, heap_memory_deleter< T > >;
	template < typename T > using shared_heap_memory = shared_memory_t< T >;

	template < typename T, typename Deleter >
	inline scoped_memory_t< T, Deleter > make_scoped_memory( T* ptr, Deleter d )
	{
		return std::move( scoped_memory_t< T, Deleter >( ptr, d ) );
	}

	template < typename T, typename Deleter >
	inline shared_memory_t< T > make_shared_memory( T* ptr, Deleter d )
	{
		return std::move( shared_memory_t< T >( ptr, d ) );
	}

	template < typename T >
	inline scoped_local_memory< T > make_scoped_local_memory( uint flags, std::size_t size )
	{
		return std::move( scoped_local_memory< T >( lcallocate< T >( flags, size ) ) );
	}

	template < typename T >
	inline shared_local_memory< T > make_shared_local_memory( uint flags, std::size_t size )
	{
		return std::move( shared_local_memory< T >( lcallocate< T >( flags, size ), local_memory_deleter< T >() ) );
	}

	template < typename T >
	inline scoped_global_memory< T > make_scoped_global_memory( uint flags, std::size_t size )
	{
		return std::move( scoped_global_memory< T >( glallocate< T >( flags, size ) ) );
	}

	template < typename T >
	inline shared_global_memory< T > make_shared_global_memory( uint flags, std::size_t size )
	{
		return std::move( shared_global_memory< T >( glallocate< T >( flags, size ), global_memory_deleter< T >() ) );
	}

	template < typename T >
	inline scoped_virtual_memory< T > make_scoped_virtual_memory( std::size_t size, dword allocationType, dword flagProtect, void* startAddr = null )
	{
		return std::move( scoped_virtual_memory< T >( vtallocate< T >( startAddr, size, allocationType, flagProtect, startAddr ) ) );
	}

	template < typename T >
	inline shared_virtual_memory< T > make_shared_virtual_memory( std::size_t size, dword allocationType, dword flagProtect, void* startAddr = null )
	{
		return std::move( shared_virtual_memory< T >( vtallocate< T >( startAddr, size, allocationType, flagProtect ), virtual_memory_deleter< T >() ) );
	}

	template < typename T >
	inline scoped_heap_memory< T > make_scoped_heap_memory( dword flags, std::size_t size, heaphadle_t hheap = ::GetProcessHeap() )
	{
		return std::move( scoped_heap_memory< T >( hpallocate< T >( flags, size, hheap ) ) );
	}

	template < typename T >
	inline shared_heap_memory< T > make_shared_heap_memory( dword flags, std::size_t size, heaphadle_t hheap = ::GetProcessHeap() )
	{
		return std::move( shared_heap_memory< T >( hpallocate< T >( flags, size, hheap ), heap_memory_deleter< T >() ) );
	}

	inline handle_t get_heap_memory_handle( void* ptr )
	{
		return Details::GetHeapMemoryHeader( ptr )->hHeap;
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using mycpp::scoped_memory_t;
using mycpp::shared_memory_t;
using mycpp::scoped_local_memory;
using mycpp::shared_local_memory;
using mycpp::scoped_global_memory;
using mycpp::shared_global_memory;
using mycpp::scoped_virtual_memory;
using mycpp::shared_virtual_memory;
using mycpp::scoped_heap_memory;
using mycpp::shared_heap_memory;
#endif
