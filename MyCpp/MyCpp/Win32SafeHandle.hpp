#pragma once
#include <memory>
#include "MyCpp/Base.hpp"
#include "MyCpp/Win32Base.hpp"

namespace MyCpp
{
	typedef HANDLE handle_t;
	typedef HKEY hkey_t;
	typedef SC_HANDLE sc_handle_t;

	const handle_t nullhandle = INVALID_HANDLE_VALUE;

	class find_wrapper
	{
	public:
		typedef HANDLE Handle;

		find_wrapper( Handle hFind )
			: m_hFind( m_hFind )
		{}

		find_wrapper() = default;
		find_wrapper( const find_wrapper& other ) = default;
		find_wrapper( find_wrapper&& other ) noexcept = default;

		operator Handle () const
		{
			return m_hFind;
		}

		Handle get_handle() const
		{
			return m_hFind;
		}

		Handle* operator & ()
		{
			return &m_hFind;
		}

		const Handle* operator & () const
		{
			return &m_hFind;
		}

		find_wrapper& operator = ( Handle hFind )
		{
			m_hFind = hFind;
		}

		find_wrapper& operator = ( const find_wrapper& other ) = default;
		find_wrapper& operator = ( find_wrapper&& other ) noexcept = default;
	private:
		Handle m_hFind = INVALID_HANDLE_VALUE;
	};

	[[nodiscard]]
	inline bool operator == ( const find_wrapper& lhs, const find_wrapper& rhs )
	{
		return ( lhs.get_handle() == rhs.get_handle() );
	}

	[[nodiscard]]
	inline bool operator != ( const find_wrapper& lhs, const find_wrapper& rhs )
	{
		return ( lhs.get_handle() != rhs.get_handle() );
	}

	[[nodiscard]]
	inline bool operator == ( const find_wrapper& lhs, find_wrapper::Handle rhs )
	{
		return ( lhs.get_handle() == rhs );
	}

	[[nodiscard]]
	inline bool operator != ( const find_wrapper& lhs, find_wrapper::Handle rhs )
	{
		return ( lhs.get_handle() != rhs );
	}

	[[nodiscard]]
	constexpr bool operator == ( const find_wrapper& lhs, const Null& )
	{
		return ( lhs.get_handle() == null );
	}

	[[nodiscard]]
	constexpr bool operator != ( const find_wrapper& lhs, const Null& )
	{
		return ( lhs.get_handle() != null );
	}

	typedef find_wrapper HFIND;
	typedef HFIND hfind_t;

	template < typename HandleType >
	struct safe_handle_closer
	{
		typedef HandleType pointer;

		void operator () ( HandleType h )
		{}
	};

	template <>
	struct safe_handle_closer< handle_t >
	{
		typedef handle_t pointer;

		void operator () ( handle_t h )
		{
			if ( h != null && h != nullhandle )
				::CloseHandle( h );
		}
	};

	template <>
	struct safe_handle_closer< hkey_t >
	{
		typedef hkey_t pointer;

		void operator () ( hkey_t h )
		{
			if ( h != null )
				::RegCloseKey( h );
		}
	};

	template <>
	struct safe_handle_closer< sc_handle_t >
	{
		typedef sc_handle_t pointer;

		void operator () ( sc_handle_t h )
		{
			if ( h != null )
				::CloseServiceHandle( h );
		}
	};

	template <>
	struct safe_handle_closer< hfind_t >
	{
		typedef hfind_t pointer;

		void operator () ( hfind_t h )
		{
			if ( h != nullhandle )
				::FindClose( h );
		}
	};

	template < typename HandleType, typename Deleter = safe_handle_closer< HandleType > >
	using scoped_handle_t = std::unique_ptr< typename std::remove_pointer< HandleType >::type, Deleter >;

	template < typename HandleType >
	using shared_handle_t = std::shared_ptr< typename std::remove_pointer< HandleType >::type >;

	typedef scoped_handle_t< handle_t >		scoped_generic_handle;
	typedef scoped_handle_t< hkey_t >		scoped_reg_handle;
	typedef scoped_handle_t< sc_handle_t >	scoped_svc_handle;

	typedef shared_handle_t< handle_t >		shared_generic_handle;
	typedef shared_handle_t< hkey_t >		shared_reg_handle;
	typedef shared_handle_t< sc_handle_t >	shared_svc_handle;

	template < typename HandleType, typename Deleter >
	inline scoped_handle_t< HandleType, Deleter > make_scoped_handle( HandleType handle, Deleter )
	{
		return scoped_handle_t< HandleType, Deleter >( handle );
	}

	template < typename HandleType >
	inline scoped_handle_t< HandleType > make_scoped_handle( HandleType handle )
	{
		return scoped_handle_t< HandleType >( handle );
	}

	template < typename HandleType, typename Deleter >
	inline shared_handle_t< HandleType > make_shared_handle( HandleType handle, Deleter deleter )
	{
		return shared_handle_t< HandleType >( handle, deleter );
	}

	template < typename HandleType >
	inline shared_handle_t< HandleType > make_shared_handle( HandleType handle )
	{
		return shared_handle_t< HandleType >( handle, safe_handle_closer< HandleType >() );
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::handle_t;
using MyCpp::hkey_t;
using MyCpp::sc_handle_t;
using MyCpp::HFIND;
using MyCpp::hfind_t;
using MyCpp::scoped_handle_t;
using MyCpp::shared_handle_t;
using MyCpp::scoped_generic_handle;
using MyCpp::scoped_reg_handle;
using MyCpp::scoped_svc_handle;
using MyCpp::shared_generic_handle;
using MyCpp::shared_reg_handle;
using MyCpp::shared_svc_handle;
#endif
