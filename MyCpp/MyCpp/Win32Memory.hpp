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
	inline T* lcallocate( unsigned int flags, std::size_t size )
	{
		T* p = reinterpret_cast< T* >( ::LocalAlloc( flags, size ) );

		if ( p == null )
			throw std::bad_alloc();

		return p;
	}

	template < typename T >
	inline T* lcallocate( unsigned int flags, std::size_t size )
	{
		T* p = reinterpret_cast< T* >( ::LocalAlloc( flags, size ) );

		if ( p == null )
			throw std::bad_alloc();

		return p;
	}

	template < typename T >
	using scoped_local_memory = std::unique_ptr< T, local_memory_deleter< T > >;

	template < typename T >
	using shared_local_memory = std::shared_ptr< T >;

	template < typename T, typename Deleter >
	using scoped_memory = std::unique_ptr< T, Deleter >;

	template < typename T, typename Deleter >
	using shared_memory = std::unique_ptr< T, Deleter >;

	template < typename T >
	scoped_local_memory< T >&& make_scoped_local_memory( unsigned int flags, std::size_t size )
	{
		return std::move( scoped_local_memory< T >( lcallocate( flags, size ) ) );
	}

	template < typename T >
	shared_local_memory< T >&& make_shared_local_memory( unsigned int flags, std::size_t size )
	{
		return std::move( shared_local_memory< T >( lcallocate( flags, size ), local_memory_deleter< T > ) );
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::scoped_local_memory;
using MyCpp::scoped_memory;
#endif
