#pragma once
#include <utility>
#include "MyCpp/Win32SafeHandle.hpp"

namespace MyCpp
{
	class hcursor_t
	{
	public:
		hcursor_t() = default;
		hcursor_t( const hcursor_t& other ) = default;
		hcursor_t( hcursor_t&& other ) noexcept = default;

		operator HCURSOR () const
		{
			return m_hcursor;
		}

		HCURSOR get_handle() const
		{
			return m_hcursor;
		}

		hcursor_t& operator = ( HCURSOR other )
		{
			m_hcursor = other;
		}

		hcursor_t& operator = ( const hcursor_t& other ) = default;
		hcursor_t& operator = ( hcursor_t&& other ) noexcept = default;
	private:
		HCURSOR m_hcursor = null;
	};

	typedef hcursor_t HCURSOR2;

	template <>
	struct safe_handle_closer< HBITMAP >
	{
		typedef HBITMAP pointer;

		void operator () ( HBITMAP h )
		{
			if ( h != null )
				::DeleteObject( reinterpret_cast< HGDIOBJ >( h ) );
		}
	};

	template <>
	struct safe_handle_closer< HACCEL >
	{
		typedef HACCEL pointer;

		void operator () ( HACCEL h )
		{
			if ( h != null )
				::DestroyAcceleratorTable( h );
		}
	};

	template <>
	struct safe_handle_closer< HCURSOR2 >
	{
		typedef HCURSOR2 pointer;

		void operator () ( HCURSOR2 h )
		{
			if ( h != null )
				::DestroyCursor( h );
		}
	};

	template <>
	struct safe_handle_closer< HICON >
	{
		typedef HICON pointer;

		void operator () ( HICON h )
		{
			if ( h != null )
				::DestroyIcon( h );
		}
	};

	template <>
	struct safe_handle_closer< HMENU >
	{
		typedef HMENU pointer;

		void operator () ( HMENU h )
		{
			if ( h != null )
				::DestroyMenu( h );
		}
	};

	inline constexpr char_t* resource_id( int id )
	{
		return MAKEINTRESOURCE( id );
	}

	inline constexpr word lang_id( word id_primary, word id_secondary )
	{
		return MAKELANGID( id_primary, id_secondary );
	}

	std::pair< void*, std::size_t > GetRawResourceData( HMODULE module, const char_t* type, const char_t* name, word language = lang_id( LANG_NEUTRAL, SUBLANG_DEFAULT ) );
	string_t GetStringResource( HINSTANCE instance, uint id );
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::HCURSOR2;
#endif
