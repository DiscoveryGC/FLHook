#pragma once

// N.B.: Must be included *after* FLHook.hpp; st6_malloc and st6_free must be defined!
#ifndef ST6_ALLOCATION_DEFINED
	#error st6_malloc and st6_free must be defined before st6.h is included!
#endif

#include <cstddef>
#include <stdexcept>
#include <iterator>

#ifndef _DESTRUCTOR
	#define _DESTRUCTOR(ty, ptr) (ptr)->~ty()
#endif
#ifndef _THROW2
	#define _THROW2(x, y) throw x(y)
#endif
#ifndef _POINTER_X
	#define _POINTER_X(T, A) T*
#endif
#ifndef _REFERENCE_X
	#define _REFERENCE_X(T, A) T&
#endif

namespace st6
{
	template<class _Ty>
	_Ty* _Allocate(ptrdiff_t _N, _Ty*)
	{
		if (_N < 0)
			_N = 0;
		return ((_Ty*)st6_malloc((size_t)_N * sizeof(_Ty)));
	}

	template<class _T1, class _T2>
	void _Construct(_T1* _P, const _T2& _V)
	{
		new ((void*)_P) _T1(_V);
	}

	template<class _Ty>
	void _Destroy(_Ty* _P)
	{
		_DESTRUCTOR(_Ty, _P);
	}

	inline void _Destroy(char* _P)
	{
	}
	inline void _Destroy(wchar_t* _P)
	{
	}

	template<class _Ty>
	class allocator
	{
	  public:
		typedef size_t size_type;
		typedef ptrdiff_t difference_type;
		typedef _Ty* pointer;
		typedef const _Ty* const_pointer;
		typedef _Ty& reference;
		typedef const _Ty& const_reference;
		typedef _Ty value_type;
		pointer address(reference _X) const { return (&_X); }
		const_pointer address(const_reference _X) const { return (&_X); }
		pointer allocate(size_type _N, const void*) { return (_Allocate((difference_type)_N, (pointer)0)); }

		char* _Charalloc(size_type _N) { return (_Allocate((difference_type)_N, (char*)0)); }

		void deallocate(void* _P, size_type) { st6_free(_P); }
		void construct(pointer _P, const _Ty& _V) { _Construct(_P, _V); }
		void destroy(pointer _P) { _Destroy(_P); }

		size_t max_size() const
		{
			size_t _N = (size_t)(-1) / sizeof(_Ty);
			return (0 < _N ? _N : 1);
		}
	};

	template<class _Ty, class _U>
	bool operator==(const allocator<_Ty>&, const allocator<_U>&)
	{
		return (true);
	}

	template<class _Ty, class _U>
	bool operator!=(const allocator<_Ty>&, const allocator<_U>&)
	{
		return (false);
	}

	template<>
	class allocator<void>
	{
	  public:
		typedef void _Ty;
		typedef _Ty* pointer;
		typedef const _Ty* const_pointer;
		typedef _Ty value_type;
	};

	template<class _Ty, class _A = allocator<_Ty>>
	class vector
	{
	  public:
		typedef vector<_Ty, _A> _Myt;
		typedef _A allocator_type;
		typedef typename _A::size_type size_type;
		typedef typename _A::difference_type difference_type;
		typedef typename _A::pointer _Tptr;
		typedef typename _A::const_pointer _Ctptr;
		typedef typename _A::reference reference;
		typedef typename _A::const_reference const_reference;
		typedef typename _A::value_type value_type;
		typedef _Tptr iterator;
		typedef _Ctptr const_iterator;

		explicit vector(const _A& _Al = _A()) : allocator(_Al), _First(0), _Last(0), _End(0) {}

		explicit vector(size_type _N, const _Ty& _V = _Ty(), const _A& _Al = _A()) : allocator(_Al)
		{
			_First = allocator.allocate(_N, (void*)0);
			_Ufill(_First, _N, _V);
			_Last = _First + _N;
			_End = _Last;
		}

		vector(const _Myt& _X) : allocator(_X.allocator)
		{
			_First = allocator.allocate(_X.size(), (void*)0);
			_Last = _Ucopy(_X.begin(), _X.end(), _First);
			_End = _Last;
		}

		typedef const_iterator _It;

		vector(_It _F, _It _L, const _A& _Al = _A()) : allocator(_Al), _First(0), _Last(0), _End(0) { insert(begin(), _F, _L); }

		~vector()
		{
			_Destroy(_First, _Last);
			allocator.deallocate(_First, _End - _First);
			_First = 0, _Last = 0, _End = 0;
		}

		_Myt& operator=(const _Myt& _X)
		{
			if (this == &_X)
				;
			else if (_X.size() <= size())
			{
				iterator _S = copy(_X.begin(), _X.end(), _First);
				_Destroy(_S, _Last);
				_Last = _First + _X.size();
			}
			else if (_X.size() <= capacity())
			{
				const_iterator _S = _X.begin() + size();
				copy(_X.begin(), _S, _First);
				_Ucopy(_S, _X.end(), _Last);
				_Last = _First + _X.size();
			}
			else
			{
				_Destroy(_First, _Last);
				allocator.deallocate(_First, _End - _First);
				_First = allocator.allocate(_X.size(), (void*)0);
				_Last = _Ucopy(_X.begin(), _X.end(), _First);
				_End = _Last;
			}
			return (*this);
		}

		void reserve(size_type _N)
		{
			if (capacity() < _N)
			{
				iterator _S = allocator.allocate(_N, (void*)0);
				_Ucopy(_First, _Last, _S);
				_Destroy(_First, _Last);
				allocator.deallocate(_First, _End - _First);
				_End = _S + _N;
				_Last = _S + size();
				_First = _S;
			}
		}

		size_type capacity() const { return (_First == 0 ? 0 : _End - _First); }
		iterator begin() { return (_First); }
		const_iterator begin() const { return ((const_iterator)_First); }
		iterator end() { return (_Last); }
		const_iterator end() const { return ((const_iterator)_Last); }

		void resize(size_type _N, const _Ty& _X = _Ty())
		{
			if (size() < _N)
				insert(end(), _N - size(), _X);
			else if (_N < size())
				erase(begin() + _N, end());
		}

		size_type size() const { return (_First == 0 ? 0 : _Last - _First); }
		size_type max_size() const { return (allocator.max_size()); }
		bool empty() const { return (size() == 0); }
		_A get_allocator() const { return (allocator); }

		const_reference at(size_type _P) const
		{
			if (size() <= _P)
				_Xran();
			return (*(begin() + _P));
		}

		reference at(size_type _P)
		{
			if (size() <= _P)
				_Xran();
			return (*(begin() + _P));
		}

		const_reference operator[](size_type _P) const { return (*(begin() + _P)); }
		reference operator[](size_type _P) { return (*(begin() + _P)); }
		reference front() { return (*begin()); }
		const_reference front() const { return (*begin()); }
		reference back() { return (*(end() - 1)); }
		const_reference back() const { return (*(end() - 1)); }
		void push_back(const _Ty& _X) { insert(end(), _X); }
		void pop_back() { erase(end() - 1); }

		void assign(_It _F, _It _L)
		{
			erase(begin(), end());
			insert(begin(), _F, _L);
		}

		void assign(size_type _N, const _Ty& _X = _Ty())
		{
			erase(begin(), end());
			insert(begin(), _N, _X);
		}

		iterator insert(iterator _P, const _Ty& _X = _Ty())
		{
			size_type _O = _P - begin();
			insert(_P, 1, _X);
			return (begin() + _O);
		}

		void insert(iterator _P, size_type _M, const _Ty& _X)
		{
			if (static_cast<uint>(_End - _Last) < _M)
			{
				size_type _N = size() + (_M < size() ? size() : _M);
				iterator _S = allocator.allocate(_N, (void*)0);
				iterator _Q = _Ucopy(_First, _P, _S);
				_Ufill(_Q, _M, _X);
				_Ucopy(_P, _Last, _Q + _M);
				_Destroy(_First, _Last);
				allocator.deallocate(_First, _End - _First);
				_End = _S + _N;
				_Last = _S + size() + _M;
				_First = _S;
			}
			else if (static_cast<uint>(_Last - _P) < _M)
			{
				_Ucopy(_P, _Last, _P + _M);
				_Ufill(_Last, _M - (_Last - _P), _X);
				fill(_P, _Last, _X);
				_Last += _M;
			}
			else if (0 < _M)
			{
				_Ucopy(_Last - _M, _Last, _Last);
				copy_backward(_P, _Last - _M, _Last);
				fill(_P, _P + _M, _X);
				_Last += _M;
			}
		}

		void insert(iterator _P, _It _F, _It _L)
		{
			size_type _M = 0;
			_Distance(_F, _L, _M);
			if (_End - _Last < _M)
			{
				size_type _N = size() + (_M < size() ? size() : _M);
				iterator _S = allocator.allocate(_N, (void*)0);
				iterator _Q = _Ucopy(_First, _P, _S);
				_Q = _Ucopy(_F, _L, _Q);
				_Ucopy(_P, _Last, _Q);
				_Destroy(_First, _Last);
				allocator.deallocate(_First, _End - _First);
				_End = _S + _N;
				_Last = _S + size() + _M;
				_First = _S;
			}
			else if (_Last - _P < _M)
			{
				_Ucopy(_P, _Last, _P + _M);
				_Ucopy(_F + (_Last - _P), _L, _Last);
				copy(_F, _F + (_Last - _P), _P);
				_Last += _M;
			}
			else if (0 < _M)
			{
				_Ucopy(_Last - _M, _Last, _Last);
				copy_backward(_P, _Last - _M, _Last);
				copy(_F, _L, _P);
				_Last += _M;
			}
		}

		iterator erase(iterator _P)
		{
			copy(_P + 1, end(), _P);
			_Destroy(_Last - 1, _Last);
			--_Last;
			return (_P);
		}

		iterator erase(iterator _F, iterator _L)
		{
			iterator _S = copy(_L, end(), _F);
			_Destroy(_S, end());
			_Last = _S;
			return (_F);
		}

		void clear() { erase(begin(), end()); }

		bool _Eq(const _Myt& _X) const { return (size() == _X.size() && equal(begin(), end(), _X.begin())); }

		bool _Lt(const _Myt& _X) const { return (lexicographical_compare(begin(), end(), _X.begin(), _X.end())); }

		void swap(_Myt& _X)
		{
			if (allocator == _X.allocator)
			{
				std::swap(_First, _X._First);
				std::swap(_Last, _X._Last);
				std::swap(_End, _X._End);
			}
			else
			{
				_Myt _Ts = *this;
				*this = _X, _X = _Ts;
			}
		}

		friend void swap(_Myt& _X, _Myt& _Y) { _X.swap(_Y); }

	  protected:
		void _Destroy(iterator _F, iterator _L)
		{
			for (; _F != _L; ++_F)
				allocator.destroy(_F);
		}

		iterator _Ucopy(const_iterator _F, const_iterator _L, iterator _P)
		{
			for (; _F != _L; ++_P, ++_F)
				allocator.construct(_P, *_F);
			return (_P);
		}

		void _Ufill(iterator _F, size_type _N, const _Ty& _X)
		{
			for (; 0 < _N; --_N, ++_F)
				allocator.construct(_F, _X);
		}

		void _Xran() const { _THROW2(std::out_of_range, "invalid vector<T> subscript"); }

		_A allocator;
		iterator _First, _Last, _End;
	};

	template<class _Ty>
	struct less
	{
		bool operator()(const _Ty& _X, const _Ty& _Y) const { return (_X < _Y); }
	};

	template<class _K, class _Ty, class _Kfn, class _Pr, class _A>
	class _Tree
	{
	  protected:
		enum _Redbl
		{
			_Red,
			_Black
		};
		struct _Node;
		friend struct _Node;
		typedef _POINTER_X(_Node, _A) _Nodeptr;
		struct _Node
		{
			_Nodeptr _Left, _Parent, _Right;
			_Ty _Value;
			_Redbl _Color;
		};
		typedef _REFERENCE_X(_Nodeptr, _A) _Nodepref;
		typedef _REFERENCE_X(const _K, _A) _Keyref;
		typedef _REFERENCE_X(_Redbl, _A) _Rbref;
		typedef _REFERENCE_X(_Ty, _A) _Vref;
		static _Rbref _Color(_Nodeptr _P) { return ((_Rbref)(*_P)._Color); }
		static _Keyref _Key(_Nodeptr _P) { return (_Kfn()(_Value(_P))); }
		static _Nodepref _Left(_Nodeptr _P) { return ((_Nodepref)(*_P)._Left); }
		static _Nodepref _Parent(_Nodeptr _P) { return ((_Nodepref)(*_P)._Parent); }
		static _Nodepref _Right(_Nodeptr _P) { return ((_Nodepref)(*_P)._Right); }
		static _Vref _Value(_Nodeptr _P) { return ((_Vref)(*_P)._Value); }

	  public:
		typedef _Tree<_K, _Ty, _Kfn, _Pr, _A> _Myt;
		typedef _K key_type;
		typedef _Ty value_type;
		typedef typename _A::size_type size_type;
		typedef typename _A::difference_type difference_type;
		typedef _POINTER_X(_Ty, _A) _Tptr;
		typedef _POINTER_X(const _Ty, _A) _Ctptr;
		typedef _REFERENCE_X(_Ty, _A) reference;
		typedef _REFERENCE_X(const _Ty, _A) const_reference;
		// CLASS const_iterator
		class iterator;
		class const_iterator;
		friend class const_iterator;
		class const_iterator
		{
		  public:
			const_iterator() {}
			const_iterator(_Nodeptr _P) : _Ptr(_P) {}
			const_iterator(const iterator& _X) : _Ptr(_X._Ptr) {}
			const_reference operator*() const { return (_Value(_Ptr)); }
			_Ctptr operator->() const { return (&**this); }
			const_iterator& operator++()
			{
				_Inc();
				return (*this);
			}
			const_iterator operator++(int)
			{
				const_iterator _Tmp = *this;
				++*this;
				return (_Tmp);
			}
			const_iterator& operator--()
			{
				_Dec();
				return (*this);
			}
			const_iterator operator--(int)
			{
				const_iterator _Tmp = *this;
				--*this;
				return (_Tmp);
			}
			bool operator==(const const_iterator& _X) const { return (_Ptr == _X._Ptr); }
			bool operator!=(const const_iterator& _X) const { return (!(*this == _X)); }
			void _Dec()
			{
				if (_Color(_Ptr) == _Red && _Parent(_Parent(_Ptr)) == _Ptr)
					_Ptr = _Right(_Ptr);
				else if (_Left(_Ptr) != _Nil)
					_Ptr = _Max(_Left(_Ptr));
				else
				{
					_Nodeptr _P;
					while (_Ptr == _Left(_P = _Parent(_Ptr)))
						_Ptr = _P;
					_Ptr = _P;
				}
			}
			void _Inc()
			{
				if (_Right(_Ptr) != _Nil)
					_Ptr = _Min(_Right(_Ptr));
				else
				{
					_Nodeptr _P;
					while (_Ptr == _Right(_P = _Parent(_Ptr)))
						_Ptr = _P;
					if (_Right(_Ptr) != _P)
						_Ptr = _P;
				}
			}
			_Nodeptr _Mynode() const { return (_Ptr); }

		  protected:
			_Nodeptr _Ptr;
		};
		// CLASS iterator
		friend class iterator;
		class iterator : public const_iterator
		{
		  public:
			iterator() {}
			iterator(_Nodeptr _P) : const_iterator(_P) {}
			reference operator*() const { return (_Value(this->_Ptr)); }
			_Tptr operator->() const { return (&**this); }
			iterator& operator++()
			{
				this->_Inc();
				return (*this);
			}
			iterator operator++(int)
			{
				iterator _Tmp = *this;
				++*this;
				return (_Tmp);
			}
			iterator& operator--()
			{
				this->_Dec();
				return (*this);
			}
			iterator operator--(int)
			{
				iterator _Tmp = *this;
				--*this;
				return (_Tmp);
			}
			bool operator==(const iterator& _X) const { return (this->_Ptr == _X._Ptr); }
			bool operator!=(const iterator& _X) const { return (!(*this == _X)); }
		};
		typedef std::pair<iterator, bool> _Pairib;
		typedef std::pair<iterator, iterator> _Pairii;
		typedef std::pair<const_iterator, const_iterator> _Paircc;
		explicit _Tree(const _Pr& _Parg, bool _Marg = true, const _A& _Al = _A()) : allocator(_Al), key_compare(_Parg), _Multi(_Marg) { _Init(); }
		_Tree(const _Ty* _F, const _Ty* _L, const _Pr& _Parg, bool _Marg = true, const _A& _Al = _A()) : allocator(_Al), key_compare(_Parg), _Multi(_Marg)
		{
			_Init();
			insert(_F, _L);
		}
		_Tree(const _Myt& _X) : allocator(_X.allocator), key_compare(_X.key_compare), _Multi(_X._Multi)
		{
			_Init();
			_Copy(_X);
		}
		~_Tree()
		{
			erase(begin(), end());
			_Freenode(_Head);
			_Head = 0, _Size = 0;
			{
				if (--_Nilrefs == 0)
				{
					_Freenode(_Nil);
					_Nil = 0;
				}
			}
		}
		_Myt& operator=(const _Myt& _X)
		{
			if (this != &_X)
			{
				erase(begin(), end());
				key_compare = _X.key_compare;
				_Copy(_X);
			}
			return (*this);
		}
		iterator begin() { return (iterator(_Lmost())); }
		const_iterator begin() const { return (const_iterator(_Lmost())); }
		iterator end() { return (iterator(_Head)); }
		const_iterator end() const { return (const_iterator(_Head)); }
		size_type size() const { return (_Size); }
		size_type max_size() const { return (allocator.max_size()); }
		bool empty() const { return (size() == 0); }
		_A get_allocator() const { return (allocator); }
		_Pr key_comp() const { return (key_compare); }
		_Pairib insert(const value_type& _V)
		{
			_Nodeptr _X = _Root();
			_Nodeptr _Y = _Head;
			bool _Ans = true;
			{
				while (_X != _Nil)
				{
					_Y = _X;
					_Ans = key_compare(_Kfn()(_V), _Key(_X));
					_X = _Ans ? _Left(_X) : _Right(_X);
				}
			}
			if (_Multi)
				return (_Pairib(_Insert(_X, _Y, _V), true));
			iterator _P = iterator(_Y);
			if (!_Ans)
				;
			else if (_P == begin())
				return (_Pairib(_Insert(_X, _Y, _V), true));
			else
				--_P;
			if (key_compare(_Key(_P._Mynode()), _Kfn()(_V)))
				return (_Pairib(_Insert(_X, _Y, _V), true));
			return (_Pairib(_P, false));
		}
		iterator insert(iterator _P, const value_type& _V)
		{
			if (size() == 0)
				;
			else if (_P == begin())
			{
				if (key_compare(_Kfn()(_V), _Key(_P._Mynode())))
					return (_Insert(_Head, _P._Mynode(), _V));
			}
			else if (_P == end())
			{
				if (key_compare(_Key(_Rmost()), _Kfn()(_V)))
					return (_Insert(_Nil, _Rmost(), _V));
			}
			else
			{
				iterator _Pb = _P;
				if (key_compare(_Key((--_Pb)._Mynode()), _Kfn()(_V)) && key_compare(_Kfn()(_V), _Key(_P._Mynode())))
				{
					if (_Right(_Pb._Mynode()) == _Nil)
						return (_Insert(_Nil, _Pb._Mynode(), _V));
					else
						return (_Insert(_Head, _P._Mynode(), _V));
				}
			}
			return (insert(_V).first);
		}
		void insert(iterator _F, iterator _L)
		{
			for (; _F != _L; ++_F)
				insert(*_F);
		}
		void insert(const value_type* _F, const value_type* _L)
		{
			for (; _F != _L; ++_F)
				insert(*_F);
		}
		iterator erase(iterator _P)
		{
			_Nodeptr _X;
			_Nodeptr _Y = (_P++)._Mynode();
			_Nodeptr _Z = _Y;
			if (_Left(_Y) == _Nil)
				_X = _Right(_Y);
			else if (_Right(_Y) == _Nil)
				_X = _Left(_Y);
			else
				_Y = _Min(_Right(_Y)), _X = _Right(_Y);
			if (_Y != _Z)
			{
				_Parent(_Left(_Z)) = _Y;
				_Left(_Y) = _Left(_Z);
				if (_Y == _Right(_Z))
					_Parent(_X) = _Y;
				else
				{
					_Parent(_X) = _Parent(_Y);
					_Left(_Parent(_Y)) = _X;
					_Right(_Y) = _Right(_Z);
					_Parent(_Right(_Z)) = _Y;
				}
				if (_Root() == _Z)
					_Root() = _Y;
				else if (_Left(_Parent(_Z)) == _Z)
					_Left(_Parent(_Z)) = _Y;
				else
					_Right(_Parent(_Z)) = _Y;
				_Parent(_Y) = _Parent(_Z);
				std::swap(_Color(_Y), _Color(_Z));
				_Y = _Z;
			}
			else
			{
				_Parent(_X) = _Parent(_Y);
				if (_Root() == _Z)
					_Root() = _X;
				else if (_Left(_Parent(_Z)) == _Z)
					_Left(_Parent(_Z)) = _X;
				else
					_Right(_Parent(_Z)) = _X;
				if (_Lmost() != _Z)
					;
				else if (_Right(_Z) == _Nil)
					_Lmost() = _Parent(_Z);
				else
					_Lmost() = _Min(_X);
				if (_Rmost() != _Z)
					;
				else if (_Left(_Z) == _Nil)
					_Rmost() = _Parent(_Z);
				else
					_Rmost() = _Max(_X);
			}
			if (_Color(_Y) == _Black)
			{
				while (_X != _Root() && _Color(_X) == _Black)
					if (_X == _Left(_Parent(_X)))
					{
						_Nodeptr _W = _Right(_Parent(_X));
						if (_Color(_W) == _Red)
						{
							_Color(_W) = _Black;
							_Color(_Parent(_X)) = _Red;
							_Lrotate(_Parent(_X));
							_W = _Right(_Parent(_X));
						}
						if (_Color(_Left(_W)) == _Black && _Color(_Right(_W)) == _Black)
						{
							_Color(_W) = _Red;
							_X = _Parent(_X);
						}
						else
						{
							if (_Color(_Right(_W)) == _Black)
							{
								_Color(_Left(_W)) = _Black;
								_Color(_W) = _Red;
								_Rrotate(_W);
								_W = _Right(_Parent(_X));
							}
							_Color(_W) = _Color(_Parent(_X));
							_Color(_Parent(_X)) = _Black;
							_Color(_Right(_W)) = _Black;
							_Lrotate(_Parent(_X));
							break;
						}
					}
					else
					{
						_Nodeptr _W = _Left(_Parent(_X));
						if (_Color(_W) == _Red)
						{
							_Color(_W) = _Black;
							_Color(_Parent(_X)) = _Red;
							_Rrotate(_Parent(_X));
							_W = _Left(_Parent(_X));
						}
						if (_Color(_Right(_W)) == _Black && _Color(_Left(_W)) == _Black)
						{
							_Color(_W) = _Red;
							_X = _Parent(_X);
						}
						else
						{
							if (_Color(_Left(_W)) == _Black)
							{
								_Color(_Right(_W)) = _Black;
								_Color(_W) = _Red;
								_Lrotate(_W);
								_W = _Left(_Parent(_X));
							}
							_Color(_W) = _Color(_Parent(_X));
							_Color(_Parent(_X)) = _Black;
							_Color(_Left(_W)) = _Black;
							_Rrotate(_Parent(_X));
							break;
						}
					}
				_Color(_X) = _Black;
			}
			_Destval(&_Value(_Y));
			_Freenode(_Y);
			--_Size;
			return (_P);
		}
		iterator erase(iterator _F, iterator _L)
		{
			if (size() == 0 || _F != begin() || _L != end())
			{
				while (_F != _L)
					erase(_F++);
				return (_F);
			}
			else
			{
				_Erase(_Root());
				_Root() = _Nil, _Size = 0;
				_Lmost() = _Head, _Rmost() = _Head;
				return (begin());
			}
		}
		size_type erase(const _K& _X)
		{
			_Pairii _P = equal_range(_X);
			size_type _N = 0;
			_Distance(_P.first, _P.second, _N);
			erase(_P.first, _P.second);
			return (_N);
		}
		void erase(const _K* _F, const _K* _L)
		{
			for (; _F != _L; ++_F)
				erase(*_F);
		}
		void clear() { erase(begin(), end()); }
		iterator find(const _K& _Kv)
		{
			iterator _P = lower_bound(_Kv);
			return (_P == end() || key_compare(_Kv, _Key(_P._Mynode())) ? end() : _P);
		}
		const_iterator find(const _K& _Kv) const
		{
			const_iterator _P = lower_bound(_Kv);
			return (_P == end() || key_compare(_Kv, _Key(_P._Mynode())) ? end() : _P);
		}
		size_type count(const _K& _Kv) const
		{
			_Paircc _Ans = equal_range(_Kv);
			size_type _N = 0;
			_Distance(_Ans.first, _Ans.second, _N);
			return (_N);
		}
		iterator lower_bound(const _K& _Kv) { return (iterator(_Lbound(_Kv))); }
		const_iterator lower_bound(const _K& _Kv) const { return (const_iterator(_Lbound(_Kv))); }
		iterator upper_bound(const _K& _Kv) { return (iterator(_Ubound(_Kv))); }
		const_iterator upper_bound(const _K& _Kv) const { return (iterator(_Ubound(_Kv))); }
		_Pairii equal_range(const _K& _Kv) { return (_Pairii(lower_bound(_Kv), upper_bound(_Kv))); }
		_Paircc equal_range(const _K& _Kv) const { return (_Paircc(lower_bound(_Kv), upper_bound(_Kv))); }
		void swap(_Myt& _X)
		{
			std::swap(key_compare, _X.key_compare);
			if (allocator == _X.allocator)
			{
				std::swap(_Head, _X._Head);
				std::swap(_Multi, _X._Multi);
				std::swap(_Size, _X._Size);
			}
			else
			{
				_Myt _Ts = *this;
				*this = _X, _X = _Ts;
			}
		}
		friend void swap(_Myt& _X, _Myt& _Y) { _X.swap(_Y); }

	  protected:
		static _Nodeptr _Nil;
		static size_t _Nilrefs;
		void _Copy(const _Myt& _X)
		{
			_Root() = _Copy(_X._Root(), _Head);
			_Size = _X.size();
			if (_Root() != _Nil)
			{
				_Lmost() = _Min(_Root());
				_Rmost() = _Max(_Root());
			}
			else
				_Lmost() = _Head, _Rmost() = _Head;
		}
		_Nodeptr _Copy(_Nodeptr _X, _Nodeptr _P)
		{
			_Nodeptr _R = _X;
			for (; _X != _Nil; _X = _Left(_X))
			{
				_Nodeptr _Y = _Buynode(_P, _Color(_X));
				if (_R == _X)
					_R = _Y;
				_Right(_Y) = _Copy(_Right(_X), _Y);
				_Consval(&_Value(_Y), _Value(_X));
				_Left(_P) = _Y;
				_P = _Y;
			}
			_Left(_P) = _Nil;
			return (_R);
		}
		void _Erase(_Nodeptr _X)
		{
			for (_Nodeptr _Y = _X; _Y != _Nil; _X = _Y)
			{
				_Erase(_Right(_Y));
				_Y = _Left(_Y);
				_Destval(&_Value(_X));
				_Freenode(_X);
			}
		}
		void _Init()
		{
			if (_Nil == 0)
			{
				_Nil = _Buynode(0, _Black);
				_Left(_Nil) = 0, _Right(_Nil) = 0;
			}
			++_Nilrefs;
			_Head = _Buynode(_Nil, _Red), _Size = 0;
			_Lmost() = _Head, _Rmost() = _Head;
		}
		iterator _Insert(_Nodeptr _X, _Nodeptr _Y, const _Ty& _V)
		{
			_Nodeptr _Z = _Buynode(_Y, _Red);
			_Left(_Z) = _Nil, _Right(_Z) = _Nil;
			_Consval(&_Value(_Z), _V);
			++_Size;
			if (_Y == _Head || _X != _Nil || key_compare(_Kfn()(_V), _Key(_Y)))
			{
				_Left(_Y) = _Z;
				if (_Y == _Head)
				{
					_Root() = _Z;
					_Rmost() = _Z;
				}
				else if (_Y == _Lmost())
					_Lmost() = _Z;
			}
			else
			{
				_Right(_Y) = _Z;
				if (_Y == _Rmost())
					_Rmost() = _Z;
			}
			for (_X = _Z; _X != _Root() && _Color(_Parent(_X)) == _Red;)
				if (_Parent(_X) == _Left(_Parent(_Parent(_X))))
				{
					_Y = _Right(_Parent(_Parent(_X)));
					if (_Color(_Y) == _Red)
					{
						_Color(_Parent(_X)) = _Black;
						_Color(_Y) = _Black;
						_Color(_Parent(_Parent(_X))) = _Red;
						_X = _Parent(_Parent(_X));
					}
					else
					{
						if (_X == _Right(_Parent(_X)))
						{
							_X = _Parent(_X);
							_Lrotate(_X);
						}
						_Color(_Parent(_X)) = _Black;
						_Color(_Parent(_Parent(_X))) = _Red;
						_Rrotate(_Parent(_Parent(_X)));
					}
				}
				else
				{
					_Y = _Left(_Parent(_Parent(_X)));
					if (_Color(_Y) == _Red)
					{
						_Color(_Parent(_X)) = _Black;
						_Color(_Y) = _Black;
						_Color(_Parent(_Parent(_X))) = _Red;
						_X = _Parent(_Parent(_X));
					}
					else
					{
						if (_X == _Left(_Parent(_X)))
						{
							_X = _Parent(_X);
							_Rrotate(_X);
						}
						_Color(_Parent(_X)) = _Black;
						_Color(_Parent(_Parent(_X))) = _Red;
						_Lrotate(_Parent(_Parent(_X)));
					}
				}
			_Color(_Root()) = _Black;
			return (iterator(_Z));
		}
		_Nodeptr _Lbound(const _K& _Kv) const
		{
			_Nodeptr _X = _Root();
			_Nodeptr _Y = _Head;
			while (_X != _Nil)
				if (key_compare(_Key(_X), _Kv))
					_X = _Right(_X);
				else
					_Y = _X, _X = _Left(_X);
			return (_Y);
		}
		_Nodeptr& _Lmost() { return (_Left(_Head)); }
		_Nodeptr& _Lmost() const { return (_Left(_Head)); }
		void _Lrotate(_Nodeptr _X)
		{
			_Nodeptr _Y = _Right(_X);
			_Right(_X) = _Left(_Y);
			if (_Left(_Y) != _Nil)
				_Parent(_Left(_Y)) = _X;
			_Parent(_Y) = _Parent(_X);
			if (_X == _Root())
				_Root() = _Y;
			else if (_X == _Left(_Parent(_X)))
				_Left(_Parent(_X)) = _Y;
			else
				_Right(_Parent(_X)) = _Y;
			_Left(_Y) = _X;
			_Parent(_X) = _Y;
		}
		static _Nodeptr _Max(_Nodeptr _P)
		{
			while (_Right(_P) != _Nil)
				_P = _Right(_P);
			return (_P);
		}
		static _Nodeptr _Min(_Nodeptr _P)
		{
			while (_Left(_P) != _Nil)
				_P = _Left(_P);
			return (_P);
		}
		_Nodeptr& _Rmost() { return (_Right(_Head)); }
		_Nodeptr& _Rmost() const { return (_Right(_Head)); }
		_Nodeptr& _Root() { return (_Parent(_Head)); }
		_Nodeptr& _Root() const { return (_Parent(_Head)); }
		void _Rrotate(_Nodeptr _X)
		{
			_Nodeptr _Y = _Left(_X);
			_Left(_X) = _Right(_Y);
			if (_Right(_Y) != _Nil)
				_Parent(_Right(_Y)) = _X;
			_Parent(_Y) = _Parent(_X);
			if (_X == _Root())
				_Root() = _Y;
			else if (_X == _Right(_Parent(_X)))
				_Right(_Parent(_X)) = _Y;
			else
				_Left(_Parent(_X)) = _Y;
			_Right(_Y) = _X;
			_Parent(_X) = _Y;
		}
		_Nodeptr _Ubound(const _K& _Kv) const
		{
			_Nodeptr _X = _Root();
			_Nodeptr _Y = _Head;
			while (_X != _Nil)
				if (key_compare(_Kv, _Key(_X)))
					_Y = _X, _X = _Left(_X);
				else
					_X = _Right(_X);
			return (_Y);
		}
		_Nodeptr _Buynode(_Nodeptr _Parg, _Redbl _Carg)
		{
			_Nodeptr _S = (_Nodeptr)allocator._Charalloc(1 * sizeof(_Node));
			_Parent(_S) = _Parg;
			_Color(_S) = _Carg;
			return (_S);
		}
		void _Consval(_Tptr _P, const _Ty& _V) { _Construct(&*_P, _V); }
		void _Destval(_Tptr _P) { _Destroy(&*_P); }
		void _Freenode(_Nodeptr _S) { allocator.deallocate(_S, 1); }
		_A allocator;
		_Pr key_compare;
		_Nodeptr _Head;
		bool _Multi;
		size_type _Size;
	};
	template<class _K, class _Ty, class _Kfn, class _Pr, class _A>
	typename _Tree<_K, _Ty, _Kfn, _Pr, _A>::_Nodeptr _Tree<_K, _Ty, _Kfn, _Pr, _A>::_Nil = 0;
	template<class _K, class _Ty, class _Kfn, class _Pr, class _A>
	size_t _Tree<_K, _Ty, _Kfn, _Pr, _A>::_Nilrefs = 0;
	template<class _K, class _Ty, class _Kfn, class _Pr, class _A>
	inline bool operator==(const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _X, const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _Y)
	{
		return (_X.size() == _Y.size() && equal(_X.begin(), _X.end(), _Y.begin()));
	}
	template<class _K, class _Ty, class _Kfn, class _Pr, class _A>
	inline bool operator!=(const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _X, const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _Y)
	{
		return (!(_X == _Y));
	}
	template<class _K, class _Ty, class _Kfn, class _Pr, class _A>
	inline bool operator<(const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _X, const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _Y)
	{
		return (lexicographical_compare(_X.begin(), _X.end(), _Y.begin(), _Y.end()));
	}
	template<class _K, class _Ty, class _Kfn, class _Pr, class _A>
	inline bool operator>(const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _X, const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _Y)
	{
		return (_Y < _X);
	}
	template<class _K, class _Ty, class _Kfn, class _Pr, class _A>
	inline bool operator<=(const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _X, const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _Y)
	{
		return (!(_Y < _X));
	}
	template<class _K, class _Ty, class _Kfn, class _Pr, class _A>
	inline bool operator>=(const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _X, const _Tree<_K, _Ty, _Kfn, _Pr, _A>& _Y)
	{
		return (!(_X < _Y));
	}

	template<class _K, class _Ty, class _Pr = st6::less<_K>, class _A = allocator<_Ty>>
	class map
	{
	  public:
		typedef map<_K, _Ty, _Pr, _A> _Myt;
		typedef std::pair<const _K, _Ty> value_type;
		struct _Kfn
		{
			const _K& operator()(const value_type& _X) const { return (_X.first); }
		};
		class value_compare
		{
			friend class map<_K, _Ty, _Pr, _A>;

		  public:
			bool operator()(const value_type& _X, const value_type& _Y) const { return (comp(_X.first, _Y.first)); }

		  protected:
			value_compare(_Pr _Pred) : comp(_Pred) {}
			_Pr comp;
		};
		typedef _K key_type;
		typedef _Ty referent_type;
		typedef _Pr key_compare;
		typedef _A allocator_type;
		typedef typename _A::reference _Tref;
		typedef _Tree<_K, value_type, _Kfn, _Pr, _A> _Imp;
		typedef typename _Imp::size_type size_type;
		typedef typename _Imp::difference_type difference_type;
		typedef typename _Imp::reference reference;
		typedef typename _Imp::const_reference const_reference;
		typedef typename _Imp::iterator iterator;
		typedef typename _Imp::const_iterator const_iterator;
		typedef std::pair<iterator, bool> _Pairib;
		typedef std::pair<iterator, iterator> _Pairii;
		typedef std::pair<const_iterator, const_iterator> _Paircc;
		explicit map(const _Pr& _Pred = _Pr(), const _A& _Al = _A()) : _Tr(_Pred, false, _Al) {}
		typedef const value_type* _It;
		map(_It _F, _It _L, const _Pr& _Pred = _Pr(), const _A& _Al = _A()) : _Tr(_Pred, false, _Al)
		{
			for (; _F != _L; ++_F)
				_Tr.insert(*_F);
		}
		_Myt& operator=(const _Myt& _X)
		{
			_Tr = _X._Tr;
			return (*this);
		}
		iterator begin() { return (_Tr.begin()); }
		const_iterator begin() const { return (_Tr.begin()); }
		iterator end() { return (_Tr.end()); }
		const_iterator end() const { return (_Tr.end()); }
		size_type size() const { return (_Tr.size()); }
		size_type max_size() const { return (_Tr.max_size()); }
		bool empty() const { return (_Tr.empty()); }
		_A get_allocator() const { return (_Tr.get_allocator()); }
		_Tref operator[](const key_type& _Kv)
		{
			iterator _P = insert(value_type(_Kv, _Ty())).first;
			return ((*_P).second);
		}
		_Pairib insert(const value_type& _X)
		{
			typename _Imp::_Pairib _Ans = _Tr.insert(_X);
			return (_Pairib(_Ans.first, _Ans.second));
		}
		iterator insert(iterator _P, const value_type& _X) { return (_Tr.insert((typename _Imp::iterator&)_P, _X)); }
		void insert(_It _F, _It _L)
		{
			for (; _F != _L; ++_F)
				_Tr.insert(*_F);
		}
		iterator erase(iterator _P) { return (_Tr.erase((typename _Imp::iterator&)_P)); }
		iterator erase(iterator _F, iterator _L) { return (_Tr.erase((typename _Imp::iterator&)_F, (typename _Imp::iterator&)_L)); }
		size_type erase(const _K& _Kv) { return (_Tr.erase(_Kv)); }
		void clear() { _Tr.clear(); }
		void swap(_Myt& _X) { std::swap(_Tr, _X._Tr); }
		friend void swap(_Myt& _X, _Myt& _Y) { _X.swap(_Y); }
		key_compare key_comp() const { return (_Tr.key_comp()); }
		value_compare value_comp() const { return (value_compare(_Tr.key_comp())); }
		iterator find(const _K& _Kv) { return (_Tr.find(_Kv)); }
		const_iterator find(const _K& _Kv) const { return (_Tr.find(_Kv)); }
		size_type count(const _K& _Kv) const { return (_Tr.count(_Kv)); }
		iterator lower_bound(const _K& _Kv) { return (_Tr.lower_bound(_Kv)); }
		const_iterator lower_bound(const _K& _Kv) const { return (_Tr.lower_bound(_Kv)); }
		iterator upper_bound(const _K& _Kv) { return (_Tr.upper_bound(_Kv)); }
		const_iterator upper_bound(const _K& _Kv) const { return (_Tr.upper_bound(_Kv)); }
		_Pairii equal_range(const _K& _Kv) { return (_Tr.equal_range(_Kv)); }
		_Paircc equal_range(const _K& _Kv) const { return (_Tr.equal_range(_Kv)); }

	  protected:
		typename _Imp _Tr;
	};
	// map TEMPLATE OPERATORS
	template<class _K, class _Ty, class _Pr, class _A>
	inline bool operator==(const map<_K, _Ty, _Pr, _A>& _X, const map<_K, _Ty, _Pr, _A>& _Y)
	{
		return (_X.size() == _Y.size() && equal(_X.begin(), _X.end(), _Y.begin()));
	}
	template<class _K, class _Ty, class _Pr, class _A>
	inline bool operator!=(const map<_K, _Ty, _Pr, _A>& _X, const map<_K, _Ty, _Pr, _A>& _Y)
	{
		return (!(_X == _Y));
	}
	template<class _K, class _Ty, class _Pr, class _A>
	inline bool operator<(const map<_K, _Ty, _Pr, _A>& _X, const map<_K, _Ty, _Pr, _A>& _Y)
	{
		return (lexicographical_compare(_X.begin(), _X.end(), _Y.begin(), _Y.end()));
	}
	template<class _K, class _Ty, class _Pr, class _A>
	inline bool operator>(const map<_K, _Ty, _Pr, _A>& _X, const map<_K, _Ty, _Pr, _A>& _Y)
	{
		return (_Y < _X);
	}
	template<class _K, class _Ty, class _Pr, class _A>
	inline bool operator<=(const map<_K, _Ty, _Pr, _A>& _X, const map<_K, _Ty, _Pr, _A>& _Y)
	{
		return (!(_Y < _X));
	}
	template<class _K, class _Ty, class _Pr, class _A>
	inline bool operator>=(const map<_K, _Ty, _Pr, _A>& _X, const map<_K, _Ty, _Pr, _A>& _Y)
	{
		return (!(_X < _Y));
	}

	template<class _Ty, class _A = allocator<_Ty>>
	class list
	{
	  protected:
		struct _Node;
		friend struct _Node;
		typedef _POINTER_X(_Node, _A) _Nodeptr;
		struct _Node
		{
			_Nodeptr _Next, _Prev;
			_Ty _Value;
		};
		struct _Acc;
		friend struct _Acc;
		struct _Acc
		{
			typedef _REFERENCE_X(_Nodeptr, _A) _Nodepref;
			typedef typename _A::reference _Vref;
			static _Nodepref _Next(_Nodeptr _P) { return ((_Nodepref)(*_P)._Next); }
			static _Nodepref _Prev(_Nodeptr _P) { return ((_Nodepref)(*_P)._Prev); }
			static _Vref _Value(_Nodeptr _P) { return ((_Vref)(*_P)._Value); }
		};

	  public:
		typedef list<_Ty, _A> _Myt;
		typedef _A allocator_type;
		typedef typename _A::size_type size_type;
		typedef typename _A::difference_type difference_type;
		typedef typename _A::pointer _Tptr;
		typedef typename _A::const_pointer _Ctptr;
		typedef typename _A::reference reference;
		typedef typename _A::const_reference const_reference;
		typedef typename _A::value_type value_type;
		// CLASS const_iterator
		class iterator;
		class const_iterator;
		friend class const_iterator;
		class const_iterator
		{
		  public:
			const_iterator() {}
			const_iterator(_Nodeptr _P) : _Ptr(_P) {}
			const_iterator(const iterator& _X) : _Ptr(_X._Ptr) {}
			const_reference operator*() const { return (_Acc::_Value(_Ptr)); }
			_Ctptr operator->() const { return (&**this); }
			const_iterator& operator++()
			{
				_Ptr = _Acc::_Next(_Ptr);
				return (*this);
			}
			const_iterator operator++(int)
			{
				const_iterator _Tmp = *this;
				++*this;
				return (_Tmp);
			}
			const_iterator& operator--()
			{
				_Ptr = _Acc::_Prev(_Ptr);
				return (*this);
			}
			const_iterator operator--(int)
			{
				const_iterator _Tmp = *this;
				--*this;
				return (_Tmp);
			}
			bool operator==(const const_iterator& _X) const { return (_Ptr == _X._Ptr); }
			bool operator!=(const const_iterator& _X) const { return (!(*this == _X)); }
			_Nodeptr _Mynode() const { return (_Ptr); }

		  protected:
			_Nodeptr _Ptr;
		};
		// CLASS iterator
		friend class iterator;
		class iterator : public const_iterator
		{
		  public:
			iterator() {}
			iterator(_Nodeptr _P) : const_iterator(_P) {}
			reference operator*() const { return (_Acc::_Value(this->_Ptr)); }
			_Tptr operator->() const { return (&**this); }
			iterator& operator++()
			{
				this->_Ptr = _Acc::_Next(this->_Ptr);
				return (*this);
			}
			iterator operator++(int)
			{
				iterator _Tmp = *this;
				++*this;
				return (_Tmp);
			}
			iterator& operator--()
			{
				this->_Ptr = _Acc::_Prev(this->_Ptr);
				return (*this);
			}
			iterator operator--(int)
			{
				iterator _Tmp = *this;
				--*this;
				return (_Tmp);
			}
			bool operator==(const iterator& _X) const { return (this->_Ptr == _X._Ptr); }
			bool operator!=(const iterator& _X) const { return (!(*this == _X)); }
		};
		explicit list(const _A& _Al = _A()) : allocator(_Al), _Head(_Buynode()), _Size(0) {}
		explicit list(size_type _N, const _Ty& _V = _Ty(), const _A& _Al = _A()) : allocator(_Al), _Head(_Buynode()), _Size(0) { insert(begin(), _N, _V); }
		list(const _Myt& _X) : allocator(_X.allocator), _Head(_Buynode()), _Size(0) { insert(begin(), _X.begin(), _X.end()); }
		list(const _Ty* _F, const _Ty* _L, const _A& _Al = _A()) : allocator(_Al), _Head(_Buynode()), _Size(0) { insert(begin(), _F, _L); }
		typedef const_iterator _It;
		list(_It _F, _It _L, const _A& _Al = _A()) : allocator(_Al), _Head(_Buynode()), _Size(0) { insert(begin(), _F, _L); }
		~list()
		{
			erase(begin(), end());
			_Freenode(_Head);
			_Head = 0, _Size = 0;
		}
		_Myt& operator=(const _Myt& _X)
		{
			if (this != &_X)
			{
				iterator _F1 = begin();
				iterator _L1 = end();
				const_iterator _F2 = _X.begin();
				const_iterator _L2 = _X.end();
				for (; _F1 != _L1 && _F2 != _L2; ++_F1, ++_F2)
					*_F1 = *_F2;
				erase(_F1, _L1);
				insert(_L1, _F2, _L2);
			}
			return (*this);
		}
		iterator begin() { return (iterator(_Acc::_Next(_Head))); }
		const_iterator begin() const { return (const_iterator(_Acc::_Next(_Head))); }
		iterator end() { return (iterator(_Head)); }
		const_iterator end() const { return (const_iterator(_Head)); }
		void resize(size_type _N, _Ty _X = _Ty())
		{
			if (size() < _N)
				insert(end(), _N - size(), _X);
			else
				while (_N < size())
					pop_back();
		}
		size_type size() const { return (_Size); }
		size_type max_size() const { return (allocator.max_size()); }
		bool empty() const { return (size() == 0); }
		_A get_allocator() const { return (allocator); }
		reference front() { return (*begin()); }
		const_reference front() const { return (*begin()); }
		reference back() { return (*(--end())); }
		const_reference back() const { return (*(--end())); }
		void push_front(const _Ty& _X) { insert(begin(), _X); }
		void pop_front() { erase(begin()); }
		void push_back(const _Ty& _X) { insert(end(), _X); }
		void pop_back() { erase(--end()); }
		void assign(_It _F, _It _L)
		{
			erase(begin(), end());
			insert(begin(), _F, _L);
		}
		void assign(size_type _N, const _Ty& _X = _Ty())
		{
			erase(begin(), end());
			insert(begin(), _N, _X);
		}
		iterator insert(iterator _P, const _Ty& _X = _Ty())
		{
			_Nodeptr _S = _P._Mynode();
			_Acc::_Prev(_S) = _Buynode(_S, _Acc::_Prev(_S));
			_S = _Acc::_Prev(_S);
			_Acc::_Next(_Acc::_Prev(_S)) = _S;
			allocator.construct(&_Acc::_Value(_S), _X);
			++_Size;
			return (iterator(_S));
		}
		void insert(iterator _P, size_type _M, const _Ty& _X)
		{
			for (; 0 < _M; --_M)
				insert(_P, _X);
		}
		void insert(iterator _P, const _Ty* _F, const _Ty* _L)
		{
			for (; _F != _L; ++_F)
				insert(_P, *_F);
		}
		void insert(iterator _P, _It _F, _It _L)
		{
			for (; _F != _L; ++_F)
				insert(_P, *_F);
		}
		iterator erase(iterator _P)
		{
			_Nodeptr _S = (_P++)._Mynode();
			_Acc::_Next(_Acc::_Prev(_S)) = _Acc::_Next(_S);
			_Acc::_Prev(_Acc::_Next(_S)) = _Acc::_Prev(_S);
			allocator.destroy(&_Acc::_Value(_S));
			_Freenode(_S);
			--_Size;
			return (_P);
		}
		iterator erase(iterator _F, iterator _L)
		{
			while (_F != _L)
				erase(_F++);
			return (_F);
		}
		void clear() { erase(begin(), end()); }
		void swap(_Myt& _X)
		{
			if (allocator == _X.allocator)
			{
				std::swap(_Head, _X._Head);
				std::swap(_Size, _X._Size);
			}
			else
			{
				iterator _P = begin();
				splice(_P, _X);
				_X.splice(_X.begin(), *this, _P, end());
			}
		}
		friend void swap(_Myt& _X, _Myt& _Y) { _X.swap(_Y); }
		void splice(iterator _P, _Myt& _X)
		{
			if (!_X.empty())
			{
				_Splice(_P, _X, _X.begin(), _X.end());
				_Size += _X._Size;
				_X._Size = 0;
			}
		}
		void splice(iterator _P, _Myt& _X, iterator _F)
		{
			iterator _L = _F;
			if (_P != _F && _P != ++_L)
			{
				_Splice(_P, _X, _F, _L);
				++_Size;
				--_X._Size;
			}
		}
		void splice(iterator _P, _Myt& _X, iterator _F, iterator _L)
		{
			if (_F != _L)
			{
				if (&_X != this)
				{
					difference_type _N = 0;
					_Distance(_F, _L, _N);
					_Size += _N;
					_X._Size -= _N;
				}
				_Splice(_P, _X, _F, _L);
			}
		}
		void remove(const _Ty& _V)
		{
			iterator _L = end();
			for (iterator _F = begin(); _F != _L;)
				if (*_F == _V)
					erase(_F++);
				else
					++_F;
		}
		void unique()
		{
			iterator _F = begin(), _L = end();
			if (_F != _L)
				for (iterator _M = _F; ++_M != _L; _M = _F)
					if (*_F == *_M)
						erase(_M);
					else
						_F = _M;
		}
		typedef std::not_equal_to<_Ty> _Pr2;
		void unique(_Pr2 _Pr)
		{
			iterator _F = begin(), _L = end();
			if (_F != _L)
				for (iterator _M = _F; ++_M != _L; _M = _F)
					if (_Pr(*_F, *_M))
						erase(_M);
					else
						_F = _M;
		}
		void merge(_Myt& _X)
		{
			if (&_X != this)
			{
				iterator _F1 = begin(), _L1 = end();
				iterator _F2 = _X.begin(), _L2 = _X.end();
				while (_F1 != _L1 && _F2 != _L2)
					if (*_F2 < *_F1)
					{
						iterator _Mid2 = _F2;
						_Splice(_F1, _X, _F2, ++_Mid2);
						_F2 = _Mid2;
					}
					else
						++_F1;
				if (_F2 != _L2)
					_Splice(_L1, _X, _F2, _L2);
				_Size += _X._Size;
				_X._Size = 0;
			}
		}
		typedef std::greater<_Ty> _Pr3;
		void merge(_Myt& _X, _Pr3 _Pr)
		{
			if (&_X != this)
			{
				iterator _F1 = begin(), _L1 = end();
				iterator _F2 = _X.begin(), _L2 = _X.end();
				while (_F1 != _L1 && _F2 != _L2)
					if (_Pr(*_F2, *_F1))
					{
						iterator _Mid2 = _F2;
						_Splice(_F1, _X, _F2, ++_Mid2);
						_F2 = _Mid2;
					}
					else
						++_F1;
				if (_F2 != _L2)
					_Splice(_L1, _X, _F2, _L2);
				_Size += _X._Size;
				_X._Size = 0;
			}
		}
		void sort()
		{
			if (2 <= size())
			{
				const size_t _MAXN = 15;
				_Myt _X(allocator), _AA[_MAXN + 1];
				size_t _N = 0;
				while (!empty())
				{
					_X.splice(_X.begin(), *this, begin());
					size_t _I;
					for (_I = 0; _I < _N && !_AA[_I].empty(); ++_I)
					{
						_AA[_I].merge(_X);
						_AA[_I].swap(_X);
					}
					if (_I == _MAXN)
						_AA[_I].merge(_X);
					else
					{
						_AA[_I].swap(_X);
						if (_I == _N)
							++_N;
					}
				}
				while (0 < _N)
					merge(_AA[--_N]);
			}
		}
		void sort(_Pr3 _Pr)
		{
			if (2 <= size())
			{
				const size_t _MAXN = 15;
				_Myt _X(allocator), _AA[_MAXN + 1];
				size_t _N = 0;
				while (!empty())
				{
					_X.splice(_X.begin(), *this, begin());
					size_t _I;
					for (_I = 0; _I < _N && !_AA[_I].empty(); ++_I)
					{
						_AA[_I].merge(_X, _Pr);
						_AA[_I].swap(_X);
					}
					if (_I == _MAXN)
						_AA[_I].merge(_X, _Pr);
					else
					{
						_AA[_I].swap(_X);
						if (_I == _N)
							++_N;
					}
				}
				while (0 < _N)
					merge(_AA[--_N], _Pr);
			}
		}
		void reverse()
		{
			if (2 <= size())
			{
				iterator _L = end();
				for (iterator _F = ++begin(); _F != _L;)
				{
					iterator _M = _F;
					_Splice(begin(), *this, _M, ++_F);
				}
			}
		}

	  protected:
		_Nodeptr _Buynode(_Nodeptr _Narg = 0, _Nodeptr _Parg = 0)
		{
			_Nodeptr _S = (_Nodeptr)allocator._Charalloc(1 * sizeof(_Node));
			_Acc::_Next(_S) = _Narg != 0 ? _Narg : _S;
			_Acc::_Prev(_S) = _Parg != 0 ? _Parg : _S;
			return (_S);
		}
		void _Freenode(_Nodeptr _S) { allocator.deallocate(_S, 1); }
		void _Splice(iterator _P, _Myt& _X, iterator _F, iterator _L)
		{
			if (allocator == _X.allocator)
			{
				_Acc::_Next(_Acc::_Prev(_L._Mynode())) = _P._Mynode();
				_Acc::_Next(_Acc::_Prev(_F._Mynode())) = _L._Mynode();
				_Acc::_Next(_Acc::_Prev(_P._Mynode())) = _F._Mynode();
				_Nodeptr _S = _Acc::_Prev(_P._Mynode());
				_Acc::_Prev(_P._Mynode()) = _Acc::_Prev(_L._Mynode());
				_Acc::_Prev(_L._Mynode()) = _Acc::_Prev(_F._Mynode());
				_Acc::_Prev(_F._Mynode()) = _S;
			}
			else
			{
				insert(_P, _F, _L);
				_X.erase(_F, _L);
			}
		}
		void _Xran() const { _THROW2(std::out_of_range, "invalid list<T> subscript"); }
		_A allocator;
		_Nodeptr _Head;
		size_type _Size;
	};
	// list TEMPLATE OPERATORS
	template<class _Ty, class _A>
	inline bool operator==(const list<_Ty, _A>& _X, const list<_Ty, _A>& _Y)
	{
		return (_X.size() == _Y.size() && equal(_X.begin(), _X.end(), _Y.begin()));
	}
	template<class _Ty, class _A>
	inline bool operator!=(const list<_Ty, _A>& _X, const list<_Ty, _A>& _Y)
	{
		return (!(_X == _Y));
	}
	template<class _Ty, class _A>
	inline bool operator<(const list<_Ty, _A>& _X, const list<_Ty, _A>& _Y)
	{
		return (lexicographical_compare(_X.begin(), _X.end(), _Y.begin(), _Y.end()));
	}
	template<class _Ty, class _A>
	inline bool operator>(const list<_Ty, _A>& _X, const list<_Ty, _A>& _Y)
	{
		return (_Y < _X);
	}
	template<class _Ty, class _A>
	inline bool operator<=(const list<_Ty, _A>& _X, const list<_Ty, _A>& _Y)
	{
		return (!(_Y < _X));
	}
	template<class _Ty, class _A>
	inline bool operator>=(const list<_Ty, _A>& _X, const list<_Ty, _A>& _Y)
	{
		return (!(_X < _Y));
	}

	typedef long streamoff;
	const streamoff _BADOFF = -1;
	typedef int streamsize;
	extern _CRTIMP const fpos_t _Fpz;
	// TEMPLATE CLASS fpos (from <streambuf>)
	template<class _St>
	class fpos
	{
		typedef fpos<_St> _Myt;

	  public:
#ifdef _MT
		fpos(streamoff _O = 0) : _Off(_O), _Fpos(_Fpz) { _State = _Stz; }
#else
		fpos(streamoff _O = 0) : _Off(_O), _Fpos(_Fpz), _State(_Stz) {}
#endif
		fpos(_St _S, fpos_t _F) : _Off(0), _Fpos(_F), _State(_S) {}
		_St state() const { return (_State); }
		void state(_St _S) { _State = _S; }
		fpos_t get_fpos_t() const { return (_Fpos); }
		operator streamoff() const { return (_Off + _FPOSOFF(_Fpos)); }
		streamoff operator-(const _Myt& _R) const { return ((streamoff) * this - (streamoff)_R); }
		_Myt& operator+=(streamoff _O)
		{
			_Off += _O;
			return (*this);
		}
		_Myt& operator-=(streamoff _O)
		{
			_Off -= _O;
			return (*this);
		}
		_Myt operator+(streamoff _O) const
		{
			_Myt _Tmp = *this;
			return (_Tmp += _O);
		}
		_Myt operator-(streamoff _O) const
		{
			_Myt _Tmp = *this;
			return (_Tmp -= _O);
		}
		bool operator==(const _Myt& _R) const { return ((streamoff) * this == (streamoff)_R); }
		bool operator!=(const _Myt& _R) const { return (!(*this == _R)); }

	  private:
		static _St _Stz;
		streamoff _Off;
		fpos_t _Fpos;
		_St _State;
	};
	template<class _St>
	_St fpos<_St>::_Stz;
	typedef fpos<mbstate_t> streampos;
	typedef streampos wstreampos;

	template<class _E>
	struct char_traits
	{
		typedef _E char_type;
		typedef _E int_type;
		typedef streampos pos_type;
		typedef streamoff off_type;
		typedef mbstate_t state_type;
		static void __cdecl assign(_E& _X, const _E& _Y) { _X = _Y; }
		static bool __cdecl eq(const _E& _X, const _E& _Y) { return (_X == _Y); }
		static bool __cdecl lt(const _E& _X, const _E& _Y) { return (_X < _Y); }
		static int __cdecl compare(const _E* _U, const _E* _V, size_t _N)
		{
			for (size_t _I = 0; _I < _N; ++_I, ++_U, ++_V)
				if (!eq(*_U, *_V))
					return (lt(*_U, *_V) ? -1 : +1);
			return (0);
		}
		static size_t __cdecl length(const _E* _U)
		{
			size_t _N;
			for (_N = 0; !eq(*_U, _E(0)); ++_U)
				++_N;
			return (_N);
		}
		static _E* __cdecl copy(_E* _U, const _E* _V, size_t _N)
		{
			_E* _S = _U;
			for (; 0 < _N; --_N, ++_U, ++_V)
				assign(*_U, *_V);
			return (_S);
		}
		static const _E* __cdecl find(const _E* _U, size_t _N, const _E& _C)
		{
			for (; 0 < _N; --_N, ++_U)
				if (eq(*_U, _C))
					return (_U);
			return (0);
		}
		static _E* __cdecl move(_E* _U, const _E* _V, size_t _N)
		{
			_E* _Ans = _U;
			if (_V < _U && _U < _V + _N)
				for (_U += _N, _V += _N; 0 < _N; --_N)
					assign(*--_U, *--_V);
			else
				for (; 0 < _N; --_N, ++_U, ++_V)
					assign(*_U, *_V);
			return (_Ans);
		}
		static _E* __cdecl assign(_E* _U, size_t _N, const _E& _C)
		{
			_E* _Ans = _U;
			for (; 0 < _N; --_N, ++_U)
				assign(*_U, _C);
			return (_Ans);
		}
		static _E __cdecl to_char_type(const int_type& _C) { return ((_E)_C); }
		static int_type __cdecl to_int_type(const _E& _C) { return ((int_type)_C); }
		static bool __cdecl eq_int_type(const int_type& _X, const int_type& _Y) { return (_X == _Y); }
		static int_type __cdecl eof() { return (EOF); }
		static int_type __cdecl not_eof(const int_type& _C) { return (_C != eof() ? _C : !eof()); }
	};

	template<class _E, class _Tr = char_traits<_E>, class _A = allocator<_E>>
	class basic_string
	{
	  public:
		typedef basic_string<_E, _Tr, _A> _Myt;
		typedef typename _A::size_type size_type;
		typedef typename _A::difference_type difference_type;
		typedef typename _A::pointer pointer;
		typedef typename _A::const_pointer const_pointer;
		typedef typename _A::reference reference;
		typedef typename _A::const_reference const_reference;
		typedef typename _A::value_type value_type;
		typedef typename _A::pointer iterator;
		typedef typename _A::const_pointer const_iterator;
		explicit basic_string(const _A& _Al = _A()) : allocator(_Al) { _Tidy(); }
		basic_string(const _Myt& _X) : allocator(_X.allocator) { _Tidy(), assign(_X, 0, npos); }
		basic_string(const _Myt& _X, size_type _P, size_type _M, const _A& _Al = _A()) : allocator(_Al) { _Tidy(), assign(_X, _P, _M); }
		basic_string(const _E* _S, size_type _N, const _A& _Al = _A()) : allocator(_Al) { _Tidy(), assign(_S, _N); }
		basic_string(const _E* _S, const _A& _Al = _A()) : allocator(_Al) { _Tidy(), assign(_S); }
		basic_string(size_type _N, _E _C, const _A& _Al = _A()) : allocator(_Al) { _Tidy(), assign(_N, _C); }
		typedef const_iterator _It;
		basic_string(_It _F, _It _L, const _A& _Al = _A()) : allocator(_Al)
		{
			_Tidy();
			assign(_F, _L);
		}
		~basic_string() { _Tidy(true); }
		typedef _Tr traits_type;
		typedef _A allocator_type;
		enum _Mref
		{
			_FROZEN = 255
		};
		static const size_type npos;
		_Myt& operator=(const _Myt& _X) { return (assign(_X)); }
		_Myt& operator=(const _E* _S) { return (assign(_S)); }
		_Myt& operator=(_E _C) { return (assign(1, _C)); }
		_Myt& operator+=(const _Myt& _X) { return (append(_X)); }
		_Myt& operator+=(const _E* _S) { return (append(_S)); }
		_Myt& operator+=(_E _C) { return (append(1, _C)); }
		_Myt& append(const _Myt& _X) { return (append(_X, 0, npos)); }
		_Myt& append(const _Myt& _X, size_type _P, size_type _M)
		{
			if (_X.size() < _P)
				_Xran();
			size_type _N = _X.size() - _P;
			if (_N < _M)
				_M = _N;
			if (npos - _Len <= _M)
				_Xlen();
			if (0 < _M && _Grow(_N = _Len + _M))
			{
				_Tr::copy(_Ptr + _Len, &_X.c_str()[_P], _M);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& append(const _E* _S, size_type _M)
		{
			if (npos - _Len <= _M)
				_Xlen();
			size_type _N;
			if (0 < _M && _Grow(_N = _Len + _M))
			{
				_Tr::copy(_Ptr + _Len, _S, _M);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& append(const _E* _S) { return (append(_S, _Tr::length(_S))); }
		_Myt& append(size_type _M, _E _C)
		{
			if (npos - _Len <= _M)
				_Xlen();
			size_type _N;
			if (0 < _M && _Grow(_N = _Len + _M))
			{
				_Tr::assign(_Ptr + _Len, _M, _C);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& append(_It _F, _It _L) { return (replace(end(), end(), _F, _L)); }
		_Myt& assign(const _Myt& _X) { return (assign(_X, 0, npos)); }
		_Myt& assign(const _Myt& _X, size_type _P, size_type _M)
		{
			if (_X.size() < _P)
				_Xran();
			size_type _N = _X.size() - _P;
			if (_M < _N)
				_N = _M;
			if (this == &_X)
				erase((size_type)(_P + _N)), erase(0, _P);
			else if (0 < _N && _N == _X.size() && _Refcnt(_X.c_str()) < _FROZEN - 1 && allocator == _X.allocator)
			{
				_Tidy(true);
				_Ptr = (_E*)_X.c_str();
				_Len = _X.size();
				_Res = _X.capacity();
				++_Refcnt(_Ptr);
			}
			else if (_Grow(_N, true))
			{
				_Tr::copy(_Ptr, &_X.c_str()[_P], _N);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& assign(const _E* _S, size_type _N)
		{
			if (_Grow(_N, true))
			{
				_Tr::copy(_Ptr, _S, _N);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& assign(const _E* _S) { return (assign(_S, _Tr::length(_S))); }
		_Myt& assign(size_type _N, _E _C)
		{
			if (_N == npos)
				_Xlen();
			if (_Grow(_N, true))
			{
				_Tr::assign(_Ptr, _N, _C);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& assign(_It _F, _It _L) { return (replace(begin(), end(), _F, _L)); }
		_Myt& insert(size_type _P0, const _Myt& _X) { return (insert(_P0, _X, 0, npos)); }
		_Myt& insert(size_type _P0, const _Myt& _X, size_type _P, size_type _M)
		{
			if (_Len < _P0 || _X.size() < _P)
				_Xran();
			size_type _N = _X.size() - _P;
			if (_N < _M)
				_M = _N;
			if (npos - _Len <= _M)
				_Xlen();
			if (0 < _M && _Grow(_N = _Len + _M))
			{
				_Tr::move(_Ptr + _P0 + _M, _Ptr + _P0, _Len - _P0);
				_Tr::copy(_Ptr + _P0, &_X.c_str()[_P], _M);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& insert(size_type _P0, const _E* _S, size_type _M)
		{
			if (_Len < _P0)
				_Xran();
			if (npos - _Len <= _M)
				_Xlen();
			size_type _N;
			if (0 < _M && _Grow(_N = _Len + _M))
			{
				_Tr::move(_Ptr + _P0 + _M, _Ptr + _P0, _Len - _P0);
				_Tr::copy(_Ptr + _P0, _S, _M);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& insert(size_type _P0, const _E* _S) { return (insert(_P0, _S, _Tr::length(_S))); }
		_Myt& insert(size_type _P0, size_type _M, _E _C)
		{
			if (_Len < _P0)
				_Xran();
			if (npos - _Len <= _M)
				_Xlen();
			size_type _N;
			if (0 < _M && _Grow(_N = _Len + _M))
			{
				_Tr::move(_Ptr + _P0 + _M, _Ptr + _P0, _Len - _P0);
				_Tr::assign(_Ptr + _P0, _M, _C);
				_Eos(_N);
			}
			return (*this);
		}
		iterator insert(iterator _P, _E _C)
		{
			size_type _P0 = _Pdif(_P, begin());
			insert(_P0, 1, _C);
			return (begin() + _P0);
		}
		void insert(iterator _P, size_type _M, _E _C)
		{
			size_type _P0 = _Pdif(_P, begin());
			insert(_P0, _M, _C);
		}
		void insert(iterator _P, _It _F, _It _L) { replace(_P, _P, _F, _L); }
		_Myt& erase(size_type _P0 = 0, size_type _M = npos)
		{
			if (_Len < _P0)
				_Xran();
			_Split();
			if (_Len - _P0 < _M)
				_M = _Len - _P0;
			if (0 < _M)
			{
				_Tr::move(_Ptr + _P0, _Ptr + _P0 + _M, _Len - _P0 - _M);
				size_type _N = _Len - _M;
				if (_Grow(_N))
					_Eos(_N);
			}
			return (*this);
		}
		iterator erase(iterator _P)
		{
			size_t _M = _Pdif(_P, begin());
			erase(_M, 1);
			return (_Psum(_Ptr, _M));
		}
		iterator erase(iterator _F, iterator _L)
		{
			size_t _M = _Pdif(_F, begin());
			erase(_M, _Pdif(_L, _F));
			return (_Psum(_Ptr, _M));
		}
		_Myt& replace(size_type _P0, size_type _N0, const _Myt& _X) { return (replace(_P0, _N0, _X, 0, npos)); }
		_Myt& replace(size_type _P0, size_type _N0, const _Myt& _X, size_type _P, size_type _M)
		{
			if (_Len < _P0 || _X.size() < _P)
				_Xran();
			if (_Len - _P0 < _N0)
				_N0 = _Len - _P0;
			size_type _N = _X.size() - _P;
			if (_N < _M)
				_M = _N;
			if (npos - _M <= _Len - _N0)
				_Xlen();
			_Split();
			size_type _Nm = _Len - _N0 - _P0;
			if (_M < _N0)
				_Tr::move(_Ptr + _P0 + _M, _Ptr + _P0 + _N0, _Nm);
			if ((0 < _M || 0 < _N0) && _Grow(_N = _Len + _M - _N0))
			{
				if (_N0 < _M)
					_Tr::move(_Ptr + _P0 + _M, _Ptr + _P0 + _N0, _Nm);
				_Tr::copy(_Ptr + _P0, &_X.c_str()[_P], _M);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& replace(size_type _P0, size_type _N0, const _E* _S, size_type _M)
		{
			if (_Len < _P0)
				_Xran();
			if (_Len - _P0 < _N0)
				_N0 = _Len - _P0;
			if (npos - _M <= _Len - _N0)
				_Xlen();
			_Split();
			size_type _Nm = _Len - _N0 - _P0;
			if (_M < _N0)
				_Tr::move(_Ptr + _P0 + _M, _Ptr + _P0 + _N0, _Nm);
			size_type _N;
			if ((0 < _M || 0 < _N0) && _Grow(_N = _Len + _M - _N0))
			{
				if (_N0 < _M)
					_Tr::move(_Ptr + _P0 + _M, _Ptr + _P0 + _N0, _Nm);
				_Tr::copy(_Ptr + _P0, _S, _M);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& replace(size_type _P0, size_type _N0, const _E* _S) { return (replace(_P0, _N0, _S, _Tr::length(_S))); }
		_Myt& replace(size_type _P0, size_type _N0, size_type _M, _E _C)
		{
			if (_Len < _P0)
				_Xran();
			if (_Len - _P0 < _N0)
				_N0 = _Len - _P0;
			if (npos - _M <= _Len - _N0)
				_Xlen();
			_Split();
			size_type _Nm = _Len - _N0 - _P0;
			if (_M < _N0)
				_Tr::move(_Ptr + _P0 + _M, _Ptr + _P0 + _N0, _Nm);
			size_type _N;
			if ((0 < _M || 0 < _N0) && _Grow(_N = _Len + _M - _N0))
			{
				if (_N0 < _M)
					_Tr::move(_Ptr + _P0 + _M, _Ptr + _P0 + _N0, _Nm);
				_Tr::assign(_Ptr + _P0, _M, _C);
				_Eos(_N);
			}
			return (*this);
		}
		_Myt& replace(iterator _F, iterator _L, const _Myt& _X) { return (replace(_Pdif(_F, begin()), _Pdif(_L, _F), _X)); }
		_Myt& replace(iterator _F, iterator _L, const _E* _S, size_type _M) { return (replace(_Pdif(_F, begin()), _Pdif(_L, _F), _S, _M)); }
		_Myt& replace(iterator _F, iterator _L, const _E* _S) { return (replace(_Pdif(_F, begin()), _Pdif(_L, _F), _S)); }
		_Myt& replace(iterator _F, iterator _L, size_type _M, _E _C) { return (replace(_Pdif(_F, begin()), _Pdif(_L, _F), _M, _C)); }
		_Myt& replace(iterator _F1, iterator _L1, _It _F2, _It _L2)
		{
			size_type _P0 = _Pdif(_F1, begin());
			size_type _M = 0;
			_Distance(_F2, _L2, _M);
			replace(_P0, _Pdif(_L1, _F1), _M, _E(0));
			for (_F1 = begin() + _P0; 0 < _M; ++_F1, ++_F2, --_M)
				*_F1 = *_F2;
			return (*this);
		}
		iterator begin()
		{
			_Freeze();
			return (_Ptr);
		}
		const_iterator begin() const { return (_Ptr); }
		iterator end()
		{
			_Freeze();
			return ((iterator)_Psum(_Ptr, _Len));
		}
		const_iterator end() const { return ((const_iterator)_Psum(_Ptr, _Len)); }
		reference at(size_type _P0)
		{
			if (_Len <= _P0)
				_Xran();
			_Freeze();
			return (_Ptr[_P0]);
		}
		const_reference at(size_type _P0) const
		{
			if (_Len <= _P0)
				_Xran();
			return (_Ptr[_P0]);
		}
		reference operator[](size_type _P0)
		{
			if (_Len < _P0 || _Ptr == 0)
				return ((reference)*_Nullstr());
			_Freeze();
			return (_Ptr[_P0]);
		}
		const_reference operator[](size_type _P0) const
		{
			if (_Ptr == 0)
				return (*_Nullstr());
			else
				return (_Ptr[_P0]);
		}
		const _E* c_str() const { return (_Ptr == 0 ? _Nullstr() : _Ptr); }
		const _E* data() const { return (c_str()); }
		size_type length() const { return (_Len); }
		size_type size() const { return (_Len); }
		size_type max_size() const
		{
			size_type _N = allocator.max_size();
			return (_N <= 2 ? 1 : _N - 2);
		}
		void resize(size_type _N, _E _C) { _N <= _Len ? erase(_N) : append(_N - _Len, _C); }
		void resize(size_type _N) { _N <= _Len ? erase(_N) : append(_N - _Len, _E(0)); }
		size_type capacity() const { return (_Res); }
		void reserve(size_type _N = 0)
		{
			if (_Res < _N)
				_Grow(_N);
		}
		bool empty() const { return (_Len == 0); }
		size_type copy(_E* _S, size_type _N, size_type _P0 = 0) const
		{
			if (_Len < _P0)
				_Xran();
			if (_Len - _P0 < _N)
				_N = _Len - _P0;
			if (0 < _N)
				_Tr::copy(_S, _Ptr + _P0, _N);
			return (_N);
		}
		void swap(_Myt& _X)
		{
			if (allocator == _X.allocator)
			{
				std::swap(_Ptr, _X._Ptr);
				std::swap(_Len, _X._Len);
				std::swap(_Res, _X._Res);
			}
			else
			{
				_Myt _Ts = *this;
				*this = _X, _X = _Ts;
			}
		}
		friend void swap(_Myt& _X, _Myt& _Y) { _X.swap(_Y); }
		size_type find(const _Myt& _X, size_type _P = 0) const { return (find(_X.c_str(), _P, _X.size())); }
		size_type find(const _E* _S, size_type _P, size_type _N) const
		{
			if (_N == 0 && _P <= _Len)
				return (_P);
			size_type _Nm;
			if (_P < _Len && _N <= (_Nm = _Len - _P))
			{
				const _E *_U, *_V;
				for (_Nm -= _N - 1, _V = _Ptr + _P; (_U = _Tr::find(_V, _Nm, *_S)) != 0; _Nm -= _U - _V + 1, _V = _U + 1)
					if (_Tr::compare(_U, _S, _N) == 0)
						return (_U - _Ptr);
			}
			return (npos);
		}
		size_type find(const _E* _S, size_type _P = 0) const { return (find(_S, _P, _Tr::length(_S))); }
		size_type find(_E _C, size_type _P = 0) const { return (find((const _E*)&_C, _P, 1)); }
		size_type rfind(const _Myt& _X, size_type _P = npos) const { return (rfind(_X.c_str(), _P, _X.size())); }
		size_type rfind(const _E* _S, size_type _P, size_type _N) const
		{
			if (_N == 0)
				return (_P < _Len ? _P : _Len);
			if (_N <= _Len)
				for (const _E* _U = _Ptr + +(_P < _Len - _N ? _P : _Len - _N);; --_U)
					if (_Tr::eq(*_U, *_S) && _Tr::compare(_U, _S, _N) == 0)
						return (_U - _Ptr);
					else if (_U == _Ptr)
						break;
			return (npos);
		}
		size_type rfind(const _E* _S, size_type _P = npos) const { return (rfind(_S, _P, _Tr::length(_S))); }
		size_type rfind(_E _C, size_type _P = npos) const { return (rfind((const _E*)&_C, _P, 1)); }
		size_type find_first_of(const _Myt& _X, size_type _P = 0) const { return (find_first_of(_X.c_str(), _P, _X.size())); }
		size_type find_first_of(const _E* _S, size_type _P, size_type _N) const
		{
			if (0 < _N && _P < _Len)
			{
				const _E* const _V = _Ptr + _Len;
				for (const _E* _U = _Ptr + _P; _U < _V; ++_U)
					if (_Tr::find(_S, _N, *_U) != 0)
						return (_U - _Ptr);
			}
			return (npos);
		}
		size_type find_first_of(const _E* _S, size_type _P = 0) const { return (find_first_of(_S, _P, _Tr::length(_S))); }
		size_type find_first_of(_E _C, size_type _P = 0) const { return (find((const _E*)&_C, _P, 1)); }
		size_type find_last_of(const _Myt& _X, size_type _P = npos) const { return (find_last_of(_X.c_str(), _P, _X.size())); }
		size_type find_last_of(const _E* _S, size_type _P, size_type _N) const
		{
			if (0 < _N && 0 < _Len)
				for (const _E* _U = _Ptr + (_P < _Len ? _P : _Len - 1);; --_U)
					if (_Tr::find(_S, _N, *_U) != 0)
						return (_U - _Ptr);
					else if (_U == _Ptr)
						break;
			return (npos);
		}
		size_type find_last_of(const _E* _S, size_type _P = npos) const { return (find_last_of(_S, _P, _Tr::length(_S))); }
		size_type find_last_of(_E _C, size_type _P = npos) const { return (rfind((const _E*)&_C, _P, 1)); }
		size_type find_first_not_of(const _Myt& _X, size_type _P = 0) const { return (find_first_not_of(_X.c_str(), _P, _X.size())); }
		size_type find_first_not_of(const _E* _S, size_type _P, size_type _N) const
		{
			if (_P < _Len)
			{
				const _E* const _V = _Ptr + _Len;
				for (const _E* _U = _Ptr + _P; _U < _V; ++_U)
					if (_Tr::find(_S, _N, *_U) == 0)
						return (_U - _Ptr);
			}
			return (npos);
		}
		size_type find_first_not_of(const _E* _S, size_type _P = 0) const { return (find_first_not_of(_S, _P, _Tr::length(_S))); }
		size_type find_first_not_of(_E _C, size_type _P = 0) const { return (find_first_not_of((const _E*)&_C, _P, 1)); }
		size_type find_last_not_of(const _Myt& _X, size_type _P = npos) const { return (find_last_not_of(_X.c_str(), _P, _X.size())); }
		size_type find_last_not_of(const _E* _S, size_type _P, size_type _N) const
		{
			if (0 < _Len)
				for (const _E* _U = _Ptr + (_P < _Len ? _P : _Len - 1);; --_U)
					if (_Tr::find(_S, _N, *_U) == 0)
						return (_U - _Ptr);
					else if (_U == _Ptr)
						break;
			return (npos);
		}
		size_type find_last_not_of(const _E* _S, size_type _P = npos) const { return (find_last_not_of(_S, _P, _Tr::length(_S))); }
		size_type find_last_not_of(_E _C, size_type _P = npos) const { return (find_last_not_of((const _E*)&_C, _P, 1)); }
		_Myt substr(size_type _P = 0, size_type _M = npos) const { return (_Myt(*this, _P, _M)); }
		int compare(const _Myt& _X) const { return (compare(0, _Len, _X.c_str(), _X.size())); }
		int compare(size_type _P0, size_type _N0, const _Myt& _X) const { return (compare(_P0, _N0, _X, 0, npos)); }
		int compare(size_type _P0, size_type _N0, const _Myt& _X, size_type _P, size_type _M) const
		{
			if (_X.size() < _P)
				_Xran();
			if (_X._Len - _P < _M)
				_M = _X._Len - _P;
			return (compare(_P0, _N0, _X.c_str() + _P, _M));
		}
		int compare(const _E* _S) const { return (compare(0, _Len, _S, _Tr::length(_S))); }
		int compare(size_type _P0, size_type _N0, const _E* _S) const { return (compare(_P0, _N0, _S, _Tr::length(_S))); }
		int compare(size_type _P0, size_type _N0, const _E* _S, size_type _M) const
		{
			if (_Len < _P0)
				_Xran();
			if (_Len - _P0 < _N0)
				_N0 = _Len - _P0;
			size_type _Ans = _Tr::compare(_Psum(_Ptr, _P0), _S, _N0 < _M ? _N0 : _M);
			return (_Ans != 0 ? _Ans : _N0 < _M ? -1 : _N0 == _M ? 0 : +1);
		}
		_A get_allocator() const { return (allocator); }

	  protected:
		_A allocator;

	  private:
		enum
		{
			_MIN_SIZE = sizeof(_E) <= 32 ? 31 : 7
		};
		void _Xran() const { _THROW2(std::out_of_range, "invalid string<T> subscript"); }
		void _Xlen() const { _THROW2(std::out_of_range, "invalid string<T> subscript"); }
		void _Copy(size_type _N)
		{
			size_type _Ns = _N | _MIN_SIZE;
			if (max_size() < _Ns)
				_Ns = _N;
			_E* _S;
			_TRY_BEGIN
			_S = allocator.allocate(_Ns + 2, (void*)0);
			_CATCH_ALL
			_Ns = _N;
			_S = allocator.allocate(_Ns + 2, (void*)0);
			_CATCH_END
			if (0 < _Len)
				_Tr::copy(_S + 1, _Ptr, _Len > _Ns ? _Ns : _Len);
			size_type _Olen = _Len;
			_Tidy(true);
			_Ptr = _S + 1;
			_Refcnt(_Ptr) = 0;
			_Res = _Ns;
			_Eos(_Olen > _Ns ? _Ns : _Olen);
		}
		void _Eos(size_type _N) { _Tr::assign(_Ptr[_Len = _N], _E(0)); }
		void _Freeze()
		{
			if (_Ptr != 0 && _Refcnt(_Ptr) != 0 && _Refcnt(_Ptr) != _FROZEN)
				_Grow(_Len);
			if (_Ptr != 0)
				_Refcnt(_Ptr) = _FROZEN;
		}
		bool _Grow(size_type _N, bool _Trim = false)
		{
			if (max_size() < _N)
				_Xlen();
			if (_Ptr != 0 && _Refcnt(_Ptr) != 0 && _Refcnt(_Ptr) != _FROZEN)
				if (_N == 0)
				{
					--_Refcnt(_Ptr), _Tidy();
					return (false);
				}
				else
				{
					_Copy(_N);
					return (true);
				}
			if (_N == 0)
			{
				if (_Trim)
					_Tidy(true);
				else if (_Ptr != 0)
					_Eos(0);
				return (false);
			}
			else
			{
				if (_Trim && (_MIN_SIZE < _Res || _Res < _N))
				{
					_Tidy(true);
					_Copy(_N);
				}
				else if (!_Trim && _Res < _N)
					_Copy(_N);
				return (true);
			}
		}
		static const _E* __cdecl _Nullstr()
		{
			static const _E _C = _E(0);
			return (&_C);
		}
		static size_type _Pdif(const_pointer _P2, const_pointer _P1) { return (_P2 == 0 ? 0 : _P2 - _P1); }
		static const_pointer _Psum(const_pointer _P, size_type _N) { return (_P == 0 ? 0 : _P + _N); }
		static pointer _Psum(pointer _P, size_type _N) { return (_P == 0 ? 0 : _P + _N); }
		unsigned char& _Refcnt(const _E* _U) { return (((unsigned char*)_U)[-1]); }
		void _Split()
		{
			if (_Ptr != 0 && _Refcnt(_Ptr) != 0 && _Refcnt(_Ptr) != _FROZEN)
			{
				_E* _Temp = _Ptr;
				_Tidy(true);
				assign(_Temp);
			}
		}
		void _Tidy(bool _Built = false)
		{
			if (!_Built || _Ptr == 0)
				;
			else if (_Refcnt(_Ptr) == 0 || _Refcnt(_Ptr) == _FROZEN)
				allocator.deallocate(_Ptr - 1, _Res + 2);
			else
				--_Refcnt(_Ptr);
			_Ptr = 0, _Len = 0, _Res = 0;
		}
		_E* _Ptr;
		size_type _Len, _Res;
	};
	template<class _E, class _Tr, class _A>
	const typename _A::size_type basic_string<_E, _Tr, _A>::npos = typename _A::size_type(-1);

	template<class _E, class _Tr, class _A>
	inline basic_string<_E, _Tr, _A> __cdecl operator+(const basic_string<_E, _Tr, _A>& _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (basic_string<_E, _Tr, _A>(_L) += _R);
	}
	template<class _E, class _Tr, class _A>
	inline basic_string<_E, _Tr, _A> __cdecl operator+(const _E* _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (basic_string<_E, _Tr, _A>(_L) += _R);
	}
	template<class _E, class _Tr, class _A>
	inline basic_string<_E, _Tr, _A> __cdecl operator+(const _E _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (basic_string<_E, _Tr, _A>(1, _L) += _R);
	}
	template<class _E, class _Tr, class _A>
	inline basic_string<_E, _Tr, _A> __cdecl operator+(const basic_string<_E, _Tr, _A>& _L, const _E* _R)
	{
		return (basic_string<_E, _Tr, _A>(_L) += _R);
	}
	template<class _E, class _Tr, class _A>
	inline basic_string<_E, _Tr, _A> __cdecl operator+(const basic_string<_E, _Tr, _A>& _L, const _E _R)
	{
		return (basic_string<_E, _Tr, _A>(_L) += _R);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator==(const basic_string<_E, _Tr, _A>& _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (_L.compare(_R) == 0);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator==(const _E* _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (_R.compare(_L) == 0);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator==(const basic_string<_E, _Tr, _A>& _L, const _E* _R)
	{
		return (_L.compare(_R) == 0);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator!=(const basic_string<_E, _Tr, _A>& _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (!(_L == _R));
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator!=(const _E* _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (!(_L == _R));
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator!=(const basic_string<_E, _Tr, _A>& _L, const _E* _R)
	{
		return (!(_L == _R));
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator<(const basic_string<_E, _Tr, _A>& _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (_L.compare(_R) < 0);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator<(const _E* _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (_R.compare(_L) > 0);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator<(const basic_string<_E, _Tr, _A>& _L, const _E* _R)
	{
		return (_L.compare(_R) < 0);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator>(const basic_string<_E, _Tr, _A>& _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (_R < _L);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator>(const _E* _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (_R < _L);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator>(const basic_string<_E, _Tr, _A>& _L, const _E* _R)
	{
		return (_R < _L);
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator<=(const basic_string<_E, _Tr, _A>& _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (!(_R < _L));
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator<=(const _E* _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (!(_R < _L));
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator<=(const basic_string<_E, _Tr, _A>& _L, const _E* _R)
	{
		return (!(_R < _L));
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator>=(const basic_string<_E, _Tr, _A>& _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (!(_L < _R));
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator>=(const _E* _L, const basic_string<_E, _Tr, _A>& _R)
	{
		return (!(_L < _R));
	}
	template<class _E, class _Tr, class _A>
	inline bool __cdecl operator>=(const basic_string<_E, _Tr, _A>& _L, const _E* _R)
	{
		return (!(_L < _R));
	}
} // namespace st6

struct ci_wchar_traits : st6::char_traits<unsigned short>
{
};
struct ci_char_traits : st6::char_traits<char>
{
};

namespace st6
{
	typedef basic_string<char, ci_char_traits, allocator<char>> string;
	typedef basic_string<unsigned short, ci_wchar_traits, allocator<unsigned short>> wstring;
} // namespace st6
