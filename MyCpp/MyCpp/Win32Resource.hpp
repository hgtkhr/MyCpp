#pragma once
#include <utility>
#include "MyCpp/Win32SafeHandle.hpp"

namespace MyCpp
{
	// Define HCURSOR as an independent type since HCURSOR is a typedef of HICON and type ambiguity resolution is not possible.
	// Design hcursor_wrapper to behave as a HCURSOR.
	class hcursor_wrapper
	{
	public:
		typedef HCURSOR Handle;

		hcursor_wrapper( Handle hCursor )
			: m_hcursor( hCursor )
		{}

		hcursor_wrapper() = default;
		hcursor_wrapper( const hcursor_wrapper& other ) = default;
		hcursor_wrapper( hcursor_wrapper&& other ) noexcept = default;

		operator Handle () const
		{
			return m_hcursor;
		}

		Handle get_handle() const
		{
			return m_hcursor;
		}

		Handle* operator & ()
		{
			return &m_hcursor;
		}

		const Handle* operator & () const
		{
			return &m_hcursor;
		}

		hcursor_wrapper& operator = ( Handle hCursor )
		{
			m_hcursor = hCursor;
		}

		hcursor_wrapper& operator = ( const hcursor_wrapper& other ) = default;
		hcursor_wrapper& operator = ( hcursor_wrapper&& other ) noexcept = default;
	private:
		HCURSOR m_hcursor = null;
	};

	[[nodiscard]]
	inline bool operator == ( const hcursor_wrapper& lhs, const hcursor_wrapper& rhs )
	{
		return ( lhs.get_handle() == rhs.get_handle() );
	}

	[[nodiscard]]
	inline bool operator != ( const hcursor_wrapper& lhs, const hcursor_wrapper& rhs )
	{
		return ( lhs.get_handle() != rhs.get_handle() );

	}
	[[nodiscard]]
	inline bool operator == ( const hcursor_wrapper& lhs, hcursor_wrapper::Handle rhs )
	{
		return ( lhs.get_handle() == rhs );
	}

	[[nodiscard]]
	inline bool operator != ( const hcursor_wrapper& lhs, hcursor_wrapper::Handle rhs )
	{
		return ( lhs.get_handle() != rhs );
	}

	[[nodiscard]]
	constexpr bool operator == ( const hcursor_wrapper& lhs, const Null& )
	{
		return ( lhs.get_handle() == null );
	}

	[[nodiscard]]
	constexpr bool operator != ( const hcursor_wrapper& lhs, const Null& )
	{
		return ( lhs.get_handle() != null );
	}

	// The return value of LoadCursor() or LoadImage() should be HCURSOR_T.
	// Otherwise, smart-pointer's delteter will not work correctly.
	typedef HBITMAP			HBITMAP_T;
	typedef HACCEL			HACCEL_T;
	typedef HICON			HICON_T;
	typedef HMENU			HMENU_T;
	typedef hcursor_wrapper	HCURSOR_T;

	template <>
	struct safe_handle_closer< HBITMAP_T >
	{
		typedef HBITMAP_T pointer;

		void operator () ( HBITMAP_T h )
		{
			if ( h != null )
				::DeleteObject( reinterpret_cast< HGDIOBJ >( h ) );
		}
	};

	template <>
	struct safe_handle_closer< HACCEL_T >
	{
		typedef HACCEL_T pointer;

		void operator () ( HACCEL_T h )
		{
			if ( h != null )
				::DestroyAcceleratorTable( h );
		}
	};

	template <>
	struct safe_handle_closer< HCURSOR_T >
	{
		typedef HCURSOR_T pointer;

		void operator () ( HCURSOR_T h )
		{
			if ( h != null )
				::DestroyCursor( h );
		}
	};

	template <>
	struct safe_handle_closer< HICON_T >
	{
		typedef HICON_T pointer;

		void operator () ( HICON_T h )
		{
			if ( h != null )
				::DestroyIcon( h );
		}
	};

	template <>
	struct safe_handle_closer< HMENU_T >
	{
		typedef HMENU_T pointer;

		void operator () ( HMENU_T h )
		{
			if ( h != null )
				::DestroyMenu( h );
		}
	};

	inline constexpr char_t* IntToResourcePtr( int id )
	{
		return MAKEINTRESOURCE( id );
	}

	inline constexpr word LangID( word id_primary, word id_secondary )
	{
		return MAKELANGID( id_primary, id_secondary );
	}

	std::pair< void*, std::size_t > GetRawResourceData( handle_t module_handle, const char_t* type, const char_t* name, word language = LangID( LANG_NEUTRAL, SUBLANG_DEFAULT ) );
	string_t GetStringResource( handle_t instance_handle, uint id );
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::HCURSOR_T;
using MyCpp::HBITMAP_T;
using MyCpp::HACCEL_T;
using MyCpp::HICON_T;
using MyCpp::HMENU_T;
using MyCpp::HCURSOR_T;
#endif

#undef LoadCursor
#undef LoadCursorFromFile
#undef CopyCursor

inline 
HCURSOR_T CreateCursor(
	MyCpp::handle_t  hInst,
	int              xHotSpot,
	int              yHotSpot,
	int              nWidth,
	int              nHeight,
	void*            pvANDPlane,
	void*            pvXORPlane
)
{
	return ::CreateCursor( 
		reinterpret_cast< HINSTANCE >( hInst ), 
		xHotSpot, 
		yHotSpot, 
		nWidth, 
		nHeight, 
		const_cast< const void* >( pvANDPlane ), 
		const_cast< const void* >( pvXORPlane ) 
	);
}

inline HCURSOR_T LoadCursor( MyCpp::handle_t hInstance, const char* lpCursorName )
{
	return ::LoadCursorA( reinterpret_cast< HINSTANCE >( hInstance ), lpCursorName );
}

inline HCURSOR_T LoadCursor( MyCpp::handle_t hInstance, const wchar_t* lpCursorName )
{
	return ::LoadCursorW( reinterpret_cast< HINSTANCE >( hInstance ), lpCursorName );
}

inline HCURSOR_T LoadCursorFromFile( const char* lpFileName )
{
	return ::LoadCursorFromFileA( lpFileName );
}

inline HCURSOR_T LoadCursorFromFile( const wchar_t* lpFileName )
{
	return ::LoadCursorFromFileW( lpFileName );
}

inline HCURSOR_T CopyCursor( HCURSOR_T cursor )
{
	return ::CopyIcon( cursor );
}
