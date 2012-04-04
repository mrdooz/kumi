#pragma once
#ifndef GWEN_CONTROLLIST_H
#define GWEN_CONTROLLIST_H

namespace Gwen
{
	struct Point;
	class TextObject;

	namespace Controls 
	{
		class Base;
	}

	template < typename TYPE >
	class TEasyList
	{
		public:

			typedef std::list<TYPE> List;

			void Add( TYPE pControl )
			{
				if ( Contains( pControl ) ) return;

				list.push_back( pControl );
			}

			void Remove( TYPE pControl )
			{
				list.remove( pControl );
			}

			void Add( const List& list )
			{
				for ( typename List::const_iterator it = list.begin(); it != list.end(); ++it )
				{
					Add( *it );
				}
			}

			bool Contains( TYPE pControl ) const
			{
				typename List::const_iterator it = std::find( list.begin(), list.end(), pControl );
				return it != list.end();
			}

			inline void Clear()
			{
				list.clear();
			}

			List list;
	};

	class ControlList : public TEasyList<Gwen::Controls::Base*>
	{
		public:

			void Enable();
			void Disable();

			void Show();
			void Hide();

			Gwen::TextObject GetValue();
			void SetValue( const Gwen::TextObject& value );

			void MoveBy( const Gwen::Point& point );

			void DoAction();
	};

};

#endif
