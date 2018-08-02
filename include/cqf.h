/*
 * ============================================================================
 *
 *       Filename:  cqf.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017-10-26 11:50:04 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prashant Pandey (), ppandey@cs.stonybrook.edu
 *   Organization:  Stony Brook University
 *
 * ============================================================================
 */

#ifndef _CQF_H_
#define _CQF_H_

#include <iostream>
#include <cassert>
#include <unordered_set>

#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <aio.h>

#include "cqf/gqf.h"
#include "util.h"

#define NUM_HASH_BITS 28
#define NUM_Q_BITS 20
#define PAGE_DROP_GRANULARITY (1ULL << 21)
#define PAGE_BUFFER_SIZE 4096

static uint64_t tmp_sum_local;

template <class key_obj>
class CQF {
	public:
		CQF();
		CQF(uint64_t qbits, uint64_t key_bits, uint32_t seed);
		CQF(std::string& filename, bool flag);
		CQF(const CQF<key_obj>& copy_cqf);

		void insert(const key_obj& k);

		/* Will return the count. */
		uint64_t query(const key_obj& k);

		uint64_t get_index(const key_obj& k);

		void serialize(std::string filename) {
			qf_serialize(&cqf, filename.c_str());
		}

		uint64_t range(void) const { return cqf.metadata->range; }
		uint32_t seed(void) const { return cqf.metadata->seed; }
		uint32_t keybits(void) const { return cqf.metadata->key_bits; }
		uint64_t size(void) const { return cqf.metadata->ndistinct_elts; }
		uint64_t capacity() const {return cqf.metadata->nslots; }
		//uint64_t set_size(void) const { return set.size(); }
		void reset(void) { qf_reset(&cqf); }

		void dump_metadata(void) const { DEBUG_DUMP(&cqf); }

		void drop_pages(uint64_t cur);

		static std::vector<QFi> qfi;
		static std::vector<typename key_obj::kmer_t> keys;
		struct Iterator {
			Iterator(uint32_t id, const CQF<key_obj>& cqf): id(id) {
				if (qfi.size() <= id) qfi.resize(id*2+1), keys.resize(id*2+1);
				if (qf_iterator(&cqf.cqf, &qfi[id], 0)) get_key();
			}
			bool advance() {
				if (qfi_next(&qfi[id])) return false;
				get_key();
				return true;
			}
			bool end() const {
				return qfi_end(&qfi[id]);
			}
			uint32_t id;
			bool operator>(const Iterator& rhs) const {
				return key() > rhs.key();
			}
			const typename key_obj::kmer_t& key() const {
				return keys[id];
			}
		private:
			void get_key() {
				uint64_t value, count;
				qfi_get(&qfi[id], &keys[id], &value, &count);
			}
		};
		friend class Iterator;

	private:
		QF cqf;
};

class KeyObject {
	public:
		KeyObject() : key(0), value(0), count(0) {};

		KeyObject(uint64_t k, uint64_t v, uint64_t c) : key(k),
		value(v), count(c) {};

		KeyObject(const KeyObject& k) : key(k.key), value(k.value), count(k.count) {};

		bool operator==(KeyObject k) { return key == k.key; }

		typedef uint64_t kmer_t;
		kmer_t key;
		uint64_t value;
		uint64_t count;
};

template <class key_obj>
CQF<key_obj>::CQF() {
	qf_init(&cqf, 1ULL << NUM_Q_BITS, NUM_HASH_BITS, 0, true, "", 23423);
}

template <class key_obj>
CQF<key_obj>::CQF(uint64_t qbits, uint64_t key_bits, uint32_t seed) {
	qf_init(&cqf, 1ULL << qbits, key_bits, 0, true, "", seed);
}

template <class key_obj>
CQF<key_obj>::CQF(std::string& filename, bool flag) {
	if (flag)
		qf_read(&cqf, filename.c_str());
	else
		qf_deserialize(&cqf, filename.c_str());
}

template <class key_obj>
CQF<key_obj>::CQF(const CQF<key_obj>& copy_cqf) {
  memcpy(reinterpret_cast<void*>(&cqf), reinterpret_cast<void*>(const_cast<QF*>(&copy_cqf.cqf)), sizeof(QF));
}

template <class key_obj>
void CQF<key_obj>::insert(const key_obj& k) {
	qf_insert(&cqf, k.key, k.value, k.count, LOCK_AND_SPIN);
	// To validate the CQF
	//set.insert(k.key);
}

template <class key_obj>
uint64_t CQF<key_obj>::query(const key_obj& k) {
	return qf_count_key_value(&cqf, k.key, k.value);
}

template <class key_obj>
uint64_t CQF<key_obj>::get_index(const key_obj& k) {
	return get_bucket_index(k.key);
}

#endif
