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

	typedef HANDLE heaphadle_t;
	typedef HANDLE memhandle_t;

	namespace details
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
				details::heapmem_header* hdr = details::GetHeapMemoryHeader( p );
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

		if ( ptr == null )
			throw std::bad_alloc();

		return ptr;
	}

	template < typename AllocFunc, typename ... Args >
	inline memhandle_t malloc_handle_func_adapter( AllocFunc allocFunc, Args&& ... args )
	{
		memhandle_t handle = reinterpret_cast< memhandle_t >( allocFunc( std::forward< Args >( args ) ... ) );

		if ( handle == null )
			throw std::bad_alloc();

		return handle;
	}

	// Non-support movable-memory.
	// To get a handle, call the *hallocate() function.
	template < typename T >
	inline T* lcallocate( uint flags, std::size_t size )
	{
		flags = ( flags & ~LMEM_MOVEABLE ) | LMEM_FIXED;
		return malloc_func_adapter< T >( &::LocalAlloc, flags, size );
	}

	template < typename T >
	inline T* glallocate( uint flags, std::size_t size )
	{
		flags = ( flags & ~GMEM_MOVEABLE ) | GMEM_FIXED;
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
		details::heapmem_header* hdr = malloc_func_adapter< details::heapmem_header* >( &::HeapAlloc, hheap, flags, sizeof( details::heapmem_header ) + size );
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
		return scoped_memory_t< T, Deleter >( ptr, d );
	}

	template < typename T, typename Deleter >
	inline shared_memory_t< T > make_shared_memory( T* ptr, Deleter d )
	{
		return shared_memory_t< T >( ptr, d );
	}

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
	inline scoped_heap_memory< T > make_scoped_heap_memory( dword flags, std::size_t size, heaphadle_t hheap = ::GetProcessHeap() )
	{
		return scoped_heap_memory< T >( hpallocate< T >( flags, size, hheap ) );
	}

	template < typename T >
	inline shared_heap_memory< T > make_shared_heap_memory( dword flags, std::size_t size, heaphadle_t hheap = ::GetProcessHeap() )
	{
		return shared_heap_memory< T >( hpallocate< T >( flags, size, hheap ), heap_memory_deleter< T >() );
	}

	inline heaphadle_t get_heap_memory_handle( void* ptr )
	{
		return details::GetHeapMemoryHeader( ptr )->hHeap;
	}

	DECLARE_HANDLE( HGLOBAL_T );
	DECLARE_HANDLE( HLOCAL_T );

	template < typename MemHandle >
	struct memlock_traits
	{};

	template <>
	struct memlock_traits< HGLOBAL_T >
	{
		typedef HGLOBAL_T internal_handle;
		typedef memhandle_t native_handle;

		static void* lock( internal_handle hMem )
		{
			return ::GlobalLock( reinterpret_cast< native_handle >( hMem ) );
		}

		static void unlock( internal_handle hMem )
		{
			::GlobalUnlock( reinterpret_cast< native_handle >( hMem ) );
		}
	};

	template <>
	struct memlock_traits< HLOCAL_T >
	{
		typedef HLOCAL_T internal_handle;
		typedef memhandle_t native_handle;

		static void* lock( internal_handle hMem )
		{
			return ::LocalLock( reinterpret_cast< native_handle >( hMem ) );
		}

		static void unlock( internal_handle hMem )
		{
			::LocalUnlock( reinterpret_cast< native_handle >( hMem ) );
		}
	};

	template < typename MemTraits >
	class memlocker
	{
	public:
		typedef MemTraits traits_type;
		typedef typename traits_type::internal_handle internal_handle;
		typedef typename traits_type::native_handle native_handle;

		memlocker() = delete;

		memlocker( native_handle hMem )
			: m_memhandle( reinterpret_cast< internal_handle > ( hMem ) )
		{}

		~memlocker()
		{
			if ( m_is_locked )
				traits_type::unlock( m_memhandle );
		}

		template < typename Pointer >
		Pointer lock()
		{
			if ( m_is_locked )
				unlock();

			Pointer ptr = static_cast< Pointer >( traits_type::lock( m_memhandle ) );

			if ( ptr != null )
				m_is_locked = true;

			return ptr;
		}

		void unlock()
		{
			traits_type::unlock( m_memhandle );
		}
	private:
		bool m_is_locked = false;
		internal_handle m_memhandle = null;
	};

	// *hallocate() function returns a handle.
	// To access a block of memory, use the *Lock() API function.
	inline memhandle_t lchallocate( uint flags, std::size_t size )
	{
		flags = ( flags & ~LMEM_FIXED ) | LMEM_MOVEABLE;
		return malloc_handle_func_adapter( &::LocalAlloc, flags, size );
	}

	inline memhandle_t glhallocate( uint flags, std::size_t size )
	{
		flags = ( flags & ~GMEM_FIXED ) | GMEM_MOVEABLE;
		return malloc_handle_func_adapter( &::LocalAlloc, flags, size );
	}

	typedef
	std::unique_ptr< typename std::remove_pointer< memhandle_t >::type, local_memory_deleter< typename std::remove_pointer< memhandle_t >::type > >
	scoped_local_memory_handle;

	typedef
	std::shared_ptr< typename std::remove_pointer< memhandle_t >::type >
	shared_local_memory_handle;

	inline scoped_local_memory_handle make_scoped_local_memory_handle( dword flags, std::size_t size )
	{
		return scoped_local_memory_handle( lchallocate( flags, size ) );
	}

	inline shared_local_memory_handle make_shared_local_memory_handle( dword flags, std::size_t size )
	{
		return { lchallocate( flags, size ), local_memory_deleter< typename std::remove_pointer< memhandle_t >::type >() };
	}

	inline memlocker< memlock_traits< HLOCAL_T > > make_scoped_local_memory_lock( const scoped_local_memory_handle& mem )
	{
		return mem.get();
	}

	inline memlocker< memlock_traits< HLOCAL_T > > make_shared_local_memory_lock( const shared_local_memory_handle& mem )
	{
		return mem.get();
	}

	typedef
	std::unique_ptr< typename std::remove_pointer< memhandle_t >::type, global_memory_deleter< typename std::remove_pointer< memhandle_t >::type > >
	scoped_global_memory_handle;

	typedef
	std::shared_ptr< typename std::remove_pointer< memhandle_t >::type >
	shared_global_memory_handle;

	inline scoped_global_memory_handle make_scoped_global_memory_handle( dword flags, std::size_t size )
	{
		return scoped_global_memory_handle( lchallocate( flags, size ) );
	}

	inline shared_global_memory_handle make_shared_global_memory_handle( dword flags, std::size_t size )
	{
		return { lchallocate( flags, size ), global_memory_deleter< typename std::remove_pointer< memhandle_t >::type >() };
	}

	inline memlocker< memlock_traits< HGLOBAL_T > > make_scoped_global_memory_lock( const scoped_global_memory_handle& mem )
	{
		return mem.get();
	}

	inline memlocker< memlock_traits< HGLOBAL_T > > make_shared_global_memory_lock( const shared_global_memory_handle& mem )
	{
		return mem.get();
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::heaphadle_t;
using MyCpp::memhandle_t;
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
using MyCpp::scoped_local_memory_handle;
using MyCpp::shared_local_memory_handle;
using MyCpp::scoped_global_memory_handle;
using MyCpp::shared_global_memory_handle;
#endif
