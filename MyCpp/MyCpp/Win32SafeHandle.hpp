#pragma once
#include <memory>
#include "MyCpp/Base.hpp"
#include "MyCpp/Win32Base.hpp"

namespace MyCpp
{
	template < typename HandleType >
	struct safe_handle_closer
	{
		typedef HandleType pointer;

		void operator () ( HandleType h )
		{}
	};

	template <>
	struct safe_handle_closer< HANDLE >
	{
		typedef HANDLE pointer;

		void operator () ( HANDLE h )
		{
			if ( h != null && h != INVALID_HANDLE_VALUE )
				::CloseHandle( h );
		}
	};

	template <>
	struct safe_handle_closer< HKEY >
	{
		typedef HKEY pointer;

		void operator () ( HKEY h )
		{
			if ( h != null )
				::RegCloseKey( h );
		}
	};

	template <>
	struct safe_handle_closer< SC_HANDLE >
	{
		typedef SC_HANDLE pointer;

		void operator () ( SC_HANDLE h )
		{
			if ( h != null )
				::CloseServiceHandle( h );
		}
	};

	template < typename HandleType, typename Deleter = safe_handle_closer< HandleType > >
	using scoped_handle = std::unique_ptr< typename std::remove_pointer< HandleType >::type, Deleter >;

	template < typename HandleType >
	using shared_handle = std::shared_ptr< typename std::remove_pointer< HandleType >::type >;

	typedef scoped_handle< HANDLE >		scoped_generic_handle;
	typedef scoped_handle< HKEY >		scoped_reg_handle;
	typedef scoped_handle< SC_HANDLE >	scoped_svc_handle;

	template < typename HandleType, typename Deleter >
	inline scoped_handle< HandleType, Deleter >&& make_scoped_handle( HandleType handle, Deleter = safe_handle_closer< HandleType >() )
	{
		return std::move( scoped_handle< HandleType, Deleter >( handle ) );
	}

	template < typename HandleType, typename Deleter >
	inline shared_handle< HandleType >&& make_shared_handle( HandleType handle, Deleter deleter = safe_handle_closer< HandleType >() )
	{
		return std::move( shared_handle< HandleType >( handle, deleter ) );
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::scoped_handle;
using MyCpp::shared_handle;
using MyCpp::scoped_generic_handle;
using MyCpp::scoped_reg_handle;
using MyCpp::scoped_svc_handle;
#endif
