#include <utility>

//
// Created by Fatemeh Almodaresi on 2019-03-07.
//

#ifndef MANTIS_MANTISSSKI_H
#define MANTIS_MANTISSSKI_H

#include "sdsl/bit_vectors.hpp"
#include "canonicalKmerIterator.h"
#include <climits>
#include "combine_kmer.h"
#include "string_view.hpp"
#include "BooPHF.h"

typedef boomphf::SingleHashFunctor<uint64_t> hasher_t;
typedef boomphf::mphf<uint64_t, hasher_t> boophf_t;

/**
 * Assumes the contig input is sorted.
 * how I have sorted the output fasta file of bcalm in bash:
 * awk 'NR%2{printf "%s\t",$0;next;}1' unitig_fasta_file > tmp
 * sort --parallel=16 -t$'\t' -k2 tmp
 * sed 's/\t/\n/g' sorted_tmp > sorted_unitig_fasta_file
 */
class MantisSski {
// unitigVec
// startIdxVec
// prefixArr (3 bits)
// MPHf
public:
    MantisSski(uint32_t kin, std::string outd, spdlog::logger* c)
    : k(kin), outdir(std::move(outd)), console(c) {}

    void buildUnitigVec(uint32_t numThreads, std::string rfile);
    void buildPrefixArr();
    void buildMPHF(uint32_t numThreads);
private:
    uint64_t contigAccumLength{0};
    uint64_t contigCnt{0};
    uint64_t nkeys{0};
    std::string outdir;
    uint32_t k{23};
    sdsl::int_vector<2> contigSeq;
    sdsl::int_vector<> contigStartIdx;
    sdsl::int_vector<3> prefixArr;
    boophf_t* bphf;
    spdlog::logger* console;
    void encodeSeq(sdsl::int_vector<2>& seqVec, size_t offset,
            stx::string_view str);

};

#endif //MANTIS_MANTISSSKI_H
