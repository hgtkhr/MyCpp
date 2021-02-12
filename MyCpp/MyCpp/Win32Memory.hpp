#pragma once
#include <memory>
#include "MyCpp/Win32Base.hpp"

namespace MyCpp
{
	template < typename T >
	struct LocalMemoryDeleter
	{
		typedef T* pointer;
		void operator () ( T* p )
		{
			if ( p != null )
				::LocalFree( reinterpret_cast< HLOCAL >( p ) );
		}
	};

	template < typename T >
	inline T* LocalAllocate( unsigned int flags, std::size_t size )
	{
		T* p = reinterpret_cast< T* >( ::LocalAlloc( flags, size ) );

		if ( p == null )
			throw std::bad_alloc();

		return p;
	}

	template < typename T >
	using ScopedLocalMemory = std::unique_ptr< T, LocalMemoryDeleter< T > >;

	template < typename T, typename Deleter >
	using ScopedMemory = std::unique_ptr< T, Deleter >;
}
