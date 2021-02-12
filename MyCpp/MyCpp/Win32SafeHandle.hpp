#pragma once
#include <memory>
#include "MyCpp/Base.hpp"
#include "MyCpp/Win32Base.hpp"

namespace MyCpp
{
	template < typename HandleType >
	struct SafeHandleCloser
	{
		typedef HandleType pointer;

		void operator () ( HandleType h )
		{}
	};

	template <>
	struct SafeHandleCloser< HANDLE >
	{
		typedef HANDLE pointer;

		void operator () ( HANDLE h )
		{
			if ( h != null && h != INVALID_HANDLE_VALUE )
				::CloseHandle( h );
		}
	};

	template <>
	struct SafeHandleCloser< HKEY >
	{
		typedef HKEY pointer;

		void operator () ( HKEY h )
		{
			if ( h != null )
				::RegCloseKey( h );
		}
	};

	template <>
	struct SafeHandleCloser< SC_HANDLE >
	{
		typedef SC_HANDLE pointer;

		void operator () ( SC_HANDLE h )
		{
			if ( h != null )
				::CloseServiceHandle( h );
		}
	};

	template < typename HandleType, typename Deleter = SafeHandleCloser< HandleType > >
	using ScopedHandle = std::unique_ptr< typename std::remove_pointer< HandleType >::type, Deleter >;

	template < typename HandleType >
	using SharedHandle = std::shared_ptr< typename std::remove_pointer< HandleType >::type >;

	typedef ScopedHandle< HANDLE > ScopedGenericHandle;
	typedef ScopedHandle< HKEY > ScopedRegHandle;
	typedef ScopedHandle< SC_HANDLE > ScopedSvcHandle;

	template < typename HandleType >
	inline SharedHandle< HandleType > MakeSharedHandle( HandleType handle )
	{
		return { handle, SafeHandleCloser< HandleType >() };
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::ScopedGenericHandle;
using MyCpp::ScopedRegHandle;
using MyCpp::ScopedSvcHandle;
#endif
