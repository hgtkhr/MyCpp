#pragma once
#include <memory>
#include "MyCpp/Win32Base.hpp"

namespace MyCpp
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

	namespace Details
	{
		struct heapmem_header
		{
			HANDLE hHeap;
			byte data[1];
		};
	}

	template < typename T >
	struct heap_memory_deleter
	{
		typedef T* pointer;
		void operator () ( T* p )
		{
			if ( p != null )
			{
				Details::heapmem_header* hdr = reinterpret_cast< heapmem_header* >( reinterpret_cast< byte* >( p ) - offsetof( Details::heapmem_header, data ) );
				HANDLE hHeap = hdr->hHeap;

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
	inline T* hpallocate( dword flags, std::size_t size, HANDLE hheap = ::GetProcessHeap() )
	{
		Details::heapmem_header* hdr = malloc_func_adapter< Details::heapmem_header* >( &::HeapAlloc, hheap, flags, size + sizeof( heapmem_header ) );
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

	template < typename T >
	inline scoped_local_memory< T > make_scoped_local_memory( uint flags, std::size_t size )
	{
		return scoped_local_memory< T >( lcallocate< T >( flags, size ) );
	}

	template < typename T >
	inline shared_local_memory< T > make_shared_local_memory( uint flags, std::size_t size )
	{
		return shared_local_memory< T >( lcallocate< T >( flags, size ), local_memory_deleter< T >() );
	}

	template < typename T >
	inline scoped_global_memory< T > make_scoped_global_memory( uint flags, std::size_t size )
	{
		return scoped_global_memory< T >( glallocate< T >( flags, size ) );
	}

	template < typename T >
	inline shared_global_memory< T > make_shared_global_memory( uint flags, std::size_t size )
	{
		return shared_global_memory< T >( glallocate< T >( flags, size ), global_memory_deleter< T >() );
	}

	template < typename T >
	inline scoped_virtual_memory< T > make_scoped_virtual_memory( std::size_t size, dword allocationType, dword flagProtect, void* startAddr = null )
	{
		return scoped_virtual_memory< T >( vtallocate< T >( startAddr, size, allocationType, flagProtect, startAddr ) );
	}

	template < typename T >
	inline shared_virtual_memory< T > make_shared_virtual_memory( std::size_t size, dword allocationType, dword flagProtect, void* startAddr = null )
	{
		return shared_virtual_memory< T >( vtallocate< T >( startAddr, size, allocationType, flagProtect ), virtual_memory_deleter< T >() );
	}

	template < typename T >
	inline scoped_heap_memory< T > make_scoped_heap_memory( dword flags, std::size_t size, HANDLE hheap = ::GetProcessHeap() )
	{
		return scoped_heap_memory< T >( hpallocate< T >( flags, size, hheap ) );
	}

	template < typename T >
	inline shared_heap_memory< T > make_shared_heap_memory( dword flags, std::size_t size, HANDLE hheap = ::GetProcessHeap() )
	{
		return shared_heap_memory< T >( hpallocate< T >( flags, size, hheap ), heap_memory_deleter< T >() );
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::scoped_memory_t;
using MyCpp::shared_memory_t;
using MyCpp::scoped_local_memory;
using MyCpp::shared_local_memory;
using MyCpp::scoped_global_memory;
using MyCpp::shared_global_memory;
using MyCpp::scoped_virtual_memory;
using MyCpp::shared_virtual_memory;
using MyCpp::scoped_heap_memory;
using MyCpp::shared_heap_memory;
#endif
