//
// Created by Fatemeh Almodaresi on 2019-05-10.
//

#include <kmer.h>
#include <gqf/hashutil.h>
#include "unitigBuilder.h"

UnitigBuilder::UnitigBuilder(std::string prefixIn, std::shared_ptr<spdlog::logger> loggerIn, uint32_t numThreads) :
        prefix(std::move(prefixIn)), nThreads(numThreads) {
    logger = loggerIn.get();
    logger->info("Reading cqf from disk.");
    std::string cqf_file(prefix + mantis::CQF_FILE);
    CQF<KeyObject> cqf(cqf_file, CQF_FREAD);
    k = cqf.keybits() / 2;
    logger->info("Done loading cdbg. k is {}", k);
    logger->info("Iterating over cqf & building unitigs ...");
    buildUnitigs(cqf);
}

std::set<dna::base> UnitigBuilder::prevNeighbors(CQF<KeyObject> &cqf, dna::kmer& node) {
    std::set<dna::base> result;
    for (const auto b : dna::bases) {
        colorIdType eqid = 0;
        if (exists(cqf, b >> node, eqid)) {
            result.insert(b);
        }
    }
    return result;
}

std::set<dna::base> UnitigBuilder::nextNeighbors(CQF<KeyObject> &cqf, dna::kmer& node) {
    std::set<dna::base> result;
    for (const auto b : dna::bases) {
        colorIdType eqid = 0;
        if (exists(cqf, node << b, eqid)) {
            result.insert(b);
        }
    }
    return result;
}

/**
 * searches for a kmer in cqf and returns the correct colorId if found
 * which is cqf count value - 1
 * @param cqf
 * @param e : search canonical kmer
 * @param eqid : reference to eqid that'll be set
 * @return true if eqid is found
 */
bool UnitigBuilder::exists(CQF<KeyObject> &cqf, dna::kmer e, colorIdType &eqid) {
    dna::canonical_kmer ck(e);
    KeyObject key(ck.val, 0, 0);
    auto eqidtmp = cqf.query(key, QF_NO_LOCK /*QF_KEY_IS_HASH | QF_NO_LOCK*/);
    if (eqidtmp) {
        eqid = eqidtmp - 1;
        return true;
    }
    return false;
}

void UnitigBuilder::buildUnitigs(CQF<KeyObject> &cqf) {
    logger->info("cqf.numslots: {}", cqf.numslots());
    logger->info("cqf.dist_elts: {}", cqf.dist_elts() );
    std::vector<bool> visited(cqf.numslots(), false);
    CQF<KeyObject>::Iterator it(cqf.begin());
    std::string filename(prefix + "/unitigs.fa");
    std::ofstream out(filename, std::ios::out);
    uint64_t cntr{1}, visitedCnt{0};
    while (!it.done()) {
        KeyObject keyobj = *it;
        KeyObject key(keyobj.key, 0, 0);
        int64_t eqidx = cqf.get_unique_index(key, 0);
        if (!visited[eqidx]) {
            dna::kmer orig_node(static_cast<int>(k), keyobj.key);
            std::string seq = std::string(orig_node);
            dna::kmer curr_node = orig_node;
            visited[eqidx] = true;
//            std::cerr << "it kmer: " << std::string(orig_node) << " " << eqidx << " v" << visited[eqidx] << "\n";
            visitedCnt++;
            auto neigbors = nextNeighbors(cqf, curr_node);
//            std::cerr << "next\n";
            while (neigbors.size() == 1) {
                dna::base n = (*neigbors.begin());
//                std::cerr << std::string(curr_node) << " ";
                curr_node = curr_node << n;
//                std::cerr << std::string(curr_node) << " ";
                dna::canonical_kmer cval(curr_node);
                eqidx = cqf.get_unique_index(KeyObject(cval.val, 0, 0), 0);
//                std::cerr << eqidx << " v" << visited[eqidx] << "\n";
                if (visited[eqidx]) break;
                seq += dna::base_to_char.at(n);
//                    auto kmerHash = hash_64(curr_node.val, BITMASK(cqf.keybits()));
//                std::cerr << "seqn:" << seq << " b:" << dna::base_to_char.at(n)
//                << " " << std::string(curr_node) << " " << eqidx << "\n";
                visited[eqidx] = true;
                visitedCnt++;

                neigbors = nextNeighbors(cqf, curr_node);
            }
/*
            std::cerr << seq << " " << neigbors.size() << "\n";
            for (auto n : neigbors)
                std::cerr << dna::base_to_char.at(n) << " ";
            std::cerr << "\n";
            std::cerr << "prevs\n";
*/
            curr_node = orig_node;
            neigbors = prevNeighbors(cqf, curr_node);
            while (neigbors.size() == 1) {
                dna::base n = (*neigbors.begin());

//                    auto kmerHash = hash_64(curr_node.val, BITMASK(cqf.keybits()));

//                std::cerr << std::string(curr_node) << " ";
                curr_node = n >> curr_node;
//                std::cerr << std::string(curr_node) << " ";
                dna::canonical_kmer cval(curr_node);
                eqidx = cqf.get_unique_index(KeyObject(cval.val, 0, 0), 0);
//                std::cerr << eqidx << " v" << visited[eqidx] << "\n";
                if (visited[eqidx]) break;
                seq = dna::base_to_char.at(n) + seq;
//                std::cerr << "seqp:" << seq << " b:" << dna::base_to_char.at(n)
//                        << " " << std::string(curr_node) << " " << eqidx << "\n";
                visited[eqidx] = true;
                visitedCnt++;
                neigbors = prevNeighbors(cqf, curr_node);
            }
           /* std::cerr << seq << " " << neigbors.size() <<"\n";
            for (auto n : neigbors)
                std::cerr << dna::base_to_char.at(n) << " ";
            std::cerr << "\n";*/
            out << ">u" << cntr << "\n";
            out << seq << "\n";
            cntr++;
        }
        ++it;
    }
    logger->info("total number of kmers (visited): {}", visitedCnt);
}

int main(int argc, char* argv[]) {
    auto console = spdlog::stdout_color_mt("mantis_console");
    UnitigBuilder unitigBuilder(argv[1], console, 16);
}