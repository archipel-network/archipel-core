#ifndef LLSORT_H_INCLUDED
#define LLSORT_H_INCLUDED

/*
 * Generic implementation of an iterative mergesort for linked lists,
 * which has a complexity of O(N * log(N)) and uses O(1) resources.
 *
 * The first parameter is the type name of the node structure,
 * the second the accessor of the compared values
 * and the third one is a pointer to the first element.
 *
 * Invocation example:
 * LLSORT(struct my_node, data->index, list);
 *
 * Attention: 'list' may be replaced by that call!
 *
 * The implementation this is based upon is available under
 * the terms and conditions noted below.
 * It can be obtained from the following web page:
 * http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
 */

/*
 * ORIGINAL LICENSE
 *
 * This file is copyright 2001 Simon Tatham.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define LLSORT_CMP_ASC(_a, _b, _cmpval) (_a->_cmpval <= _b->_cmpval)
#define LLSORT_CMP_DESC(_a, _b, _cmpval) (_b->_cmpval <= _a->_cmpval)

#define LLSORT(_typename, _cmpval, _head) \
	LLSORT_X(_typename, _cmpval, _head, LLSORT_CMP_ASC)
#define LLSORT_DESC(_typename, _cmpval, _head) \
	LLSORT_X(_typename, _cmpval, _head, LLSORT_CMP_DESC)

#define LLSORT_X(_typename, _cmpval, _head, _cmpfun) do { \
	_typename *_list, *_p, *_q, *_e, *_tail; \
	int _insize, _nmerges, _psize, _qsize, _i; \
	if (!(_head)) \
		break; \
	_list = (_head); \
	_insize = 1; \
	for (;;) { \
		_p = _list; \
		_list = NULL; \
		_tail = NULL; \
		/* count number of merges we do in this pass */ \
		_nmerges = 0; \
		while (_p) { \
			/* there exists a merge to be done */ \
			_nmerges++; \
			/* step `insize' places along from p */ \
			_q = _p; \
			_psize = 0; \
			for (_i = 0; _i < _insize; _i++) { \
				_psize++; \
				_q = _q->next; \
				if (!_q) \
					break; \
			} \
			/* if q hasn't fallen off end, \
			 * we have two lists to merge */ \
			_qsize = _insize; \
			/* now we have two lists; merge them */ \
			while (_psize > 0 || (_qsize > 0 && _q)) { \
				/* decide whether next element of merge \
				 * comes from p or q */ \
				if (_psize == 0) { \
					/* p is empty; e must come from q. */ \
					_e = _q; \
					_q = _q->next; \
					_qsize--; \
				} else if (_qsize == 0 || !_q) { \
					/* q is empty; e must come from p. */ \
					_e = _p; \
					_p = _p->next; \
					_psize--; \
				} else if (_cmpfun(_p, _q, _cmpval)) { \
					/* First element of p is lower \
					 * (or same); e must come from p. */ \
					_e = _p; \
					_p = _p->next; \
					_psize--; \
				} else { \
					/* First element of q is lower; \
					 * e must come from q. */ \
					_e = _q; \
					_q = _q->next; \
					_qsize--; \
				} \
				/* add the next element to the merged list */ \
				if (_tail) \
					_tail->next = _e; \
				else \
					_list = _e; \
				_tail = _e; \
			} \
			/* now p has stepped `insize' places along, \
			 * and q has too */ \
			_p = _q; \
		} \
		_tail->next = NULL; \
		/* If we have done only one merge, we're finished. */ \
		if (_nmerges <= 1) { \
			/* allow for nmerges == 0, the empty list case */ \
			(_head) = _list; \
			break; \
		} \
		/* Otherwise repeat, merging lists twice the size */ \
		_insize *= 2; \
	} \
} while (0)

#endif /* LLSORT_H_INCLUDED */
