/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <cstddef>
#include <cstdint>

template<typename T> class bit_history_t {
public:
	bit_history_t() :
		_history(0) {
	}

	void push(const uint_fast8_t bit) {
		_history <<= 1;
		_history |= bit & 1;
	}

	bool match(const T pattern, const size_t length) const {
		const uint32_t mask = ((T)1 << length) - 1;
		return (_history & mask) == pattern;
	}

private:
	T _history;
};

class bit_buffer_t {
public:
	bit_buffer_t(uint8_t* buffer) :
		_buffer(buffer),
		_count(0),
		_cache(0) {
	}

	~bit_buffer_t() {
		close();
	}

	void push(const uint_fast8_t bit) {
		_cache <<= 1;
		_cache |= bit & 1;
		_count += 1;
		if( (_count & 7) == 0 ) {
			flush();
		}
	}

	uint32_t extract(const size_t start, const size_t length) const {
		uint32_t result = 0;
		for(size_t n=start; n<(start+length); n++) {
			const size_t byte_index = n >> 3;
			const size_t bit_index = (n & 7) ^ 7;
			const uint32_t value = (_buffer[byte_index] >> bit_index) & 1;
			result <<= 1;
			result |= value;
		}
		return result;
	}

	void close() {
		if( (_count & 7) != 0 ) {
			flush();
		}
	}

	size_t size() const {
		return _count;
	}

private:
	uint8_t* _buffer;
	size_t _count;
	uint8_t _cache;

	void flush() {
		*(_buffer++) = _cache;
		_cache = 0;
	}
};

class bit_reader_t {
public:
	bit_reader_t(const uint8_t* const buffer) :
		_buffer(buffer),
		_position(0) {
	}

	// TODO: Add length and iterator support.

	uint32_t read() {
		const size_t byte_index = _position >> 3;
		const size_t bit_index = (_position & 7) ^ 7;
		_position += 1;
		return (_buffer[byte_index] >> bit_index) & 1;
	}

private:
	const uint8_t* const _buffer;
	size_t _position;
};
