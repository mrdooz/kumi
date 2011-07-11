#pragma once

template<class T, class Cmp = std::less<T> >
class Heap {
public:
	Heap(int capacity)
		: _data(new T[capacity])
		, _capacity(capacity)
		, _size(0)
	{
	}

	~Heap() {
		SAFE_DELETE(_data);
	}

	int left_child(int idx) { return idx*2+1; }
	int right_child(int idx) { return idx*2+2; }
	int parent(int idx) { return (idx-1) / 2; }

	bool peek(T **out) {
		if (empty())
			return false;

		*out = &_data[0];
		return true;
	}

	bool pop(T *out) {
		if (empty())
			return false;

		*out = _data[0];
		if (--_size == 0)
			return true;

		// preserve the heap property by moving the bottom element to the
		// front, and sifting it down
		_data[0] = _data[_size];
		int cur = 0;
		while (true) {
			const int l = left_child(cur);
			const int r = right_child(cur);

			int cand = -1;
			// find the lesser of the two children
			if (l <= _size && cmp(_data[l], _data[cur]))
				cand = l;

			if (r <= _size && cmp(_data[r], _data[cur]) && cmp(_data[r], _data[l]))
				cand = r;

			if (cand != -1) {
				swap(_data[cur], _data[cand]);
				cur = cand;
			} else {
				break;
			}
		}
		return true;
	}

	bool push(const T &t) {
		if (full())
			return false;

		// insert at the end, and sift up until the property holds
		_data[_size] = t;
		int cur = _size;
		while (cur) {
			const int p = parent(cur);
			if (cmp(_data[cur], _data[p])) {
				swap(_data[cur], _data[p]);
				cur = p;
			} else {
				break;
			}
		}

		_size++;
		return true;
	}

	bool empty() const { return _size == 0; }
	bool full() const { return _size == _capacity; }

private:
	Cmp cmp;
	T *_data;
	int _capacity;
	int _size;
};
