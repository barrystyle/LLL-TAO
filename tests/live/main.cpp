/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLC/types/uint1024.h>
#include <LLC/hash/SK.h>
#include <LLC/include/random.h>

#include <LLD/include/global.h>
#include <LLD/cache/binary_lru.h>
#include <LLD/keychain/hashmap.h>
#include <LLD/templates/sector.h>

#include <Util/include/debug.h>
#include <Util/include/base64.h>

#include <openssl/rand.h>

#include <LLC/hash/argon2.h>
#include <LLC/hash/SK.h>
#include <LLC/include/flkey.h>
#include <LLC/types/bignum.h>

#include <Util/include/hex.h>

#include <iostream>

#include <TAO/Register/types/address.h>
#include <TAO/Register/types/object.h>

#include <TAO/Register/include/create.h>

#include <TAO/Ledger/types/genesis.h>
#include <TAO/Ledger/types/sigchain.h>
#include <TAO/Ledger/types/transaction.h>

#include <TAO/Ledger/include/ambassador.h>

#include <Legacy/types/address.h>
#include <Legacy/types/transaction.h>

#include <LLP/templates/ddos.h>
#include <Util/include/runtime.h>

#include <list>
#include <functional>
#include <variant>

#include <Util/include/softfloat.h>
#include <Util/include/filesystem.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/stake.h>

#include <LLP/types/tritium.h>

#include <TAO/Ledger/include/chainstate.h>
#include <TAO/Ledger/types/locator.h>

#include <LLD/config/hashmap.h>
#include <LLD/config/sector.h>

class TestDB : public LLD::Templates::SectorDatabase<LLD::BinaryHashMap, LLD::BinaryLRU, LLD::Config::Hashmap>
{
public:
    TestDB(const LLD::Config::Sector& sector, const LLD::Config::Hashmap& keychain)
    : SectorDatabase(sector, keychain)
    {
    }

    ~TestDB()
    {

    }

    bool WriteKey(const uint1024_t& key, const uint1024_t& value)
    {
        return Write(std::make_pair(std::string("key"), key), value);
    }


    bool ReadKey(const uint1024_t& key, uint1024_t &value)
    {
        return Read(std::make_pair(std::string("key"), key), value);
    }


    bool EraseKey(const uint1024_t& key)
    {
        return Erase(std::make_pair(std::string("key"), key));
    }


    bool WriteLast(const uint1024_t& last)
    {
        return Write(std::string("last"), last);
    }

    bool ReadLast(uint1024_t &last)
    {
        return Read(std::string("last"), last);
    }

};

#include <TAO/Ledger/include/genesis_block.h>

const uint256_t hashSeed = 55;

#include <bitset>

#include <LLP/include/global.h>

#include <LLD/cache/template_lru.h>


/* Read an index entry at given bucket crossing file boundaries. */
bool read_index(const uint32_t nBucket, const uint32_t nTotal, const LLD::Config::Hashmap& CONFIG)
{
    const uint32_t INDEX_FILTER_SIZE = 10;

    /* Check we are flushing within range of the hashmap indexes. */
    if(nBucket >= CONFIG.HASHMAP_TOTAL_BUCKETS)
        return debug::error(FUNCTION, "out of range ", VARIABLE(nBucket));

    /* Keep track of how many buckets and bytes we have remaining in this read cycle. */
    uint32_t nRemaining = nTotal;
    uint32_t nIterator  = nBucket;

    /* Adjust our buffer size to fit the total buckets. */
    do
    {
        /* Calculate the file and boundaries we are on with current bucket. */
        const uint32_t nFile = (nIterator * CONFIG.MAX_FILES_PER_INDEX) / CONFIG.HASHMAP_TOTAL_BUCKETS;
        const uint32_t nEnd  = (((nFile + 1) * CONFIG.HASHMAP_TOTAL_BUCKETS) / CONFIG.MAX_FILES_PER_INDEX) + 1;

        /* Find our new file position from current bucket and offset. */
        const uint64_t nFilePos      = (INDEX_FILTER_SIZE * (nIterator - //(nFile == 0 ? 0 : 1) -
            ((nFile * CONFIG.HASHMAP_TOTAL_BUCKETS) / CONFIG.MAX_FILES_PER_INDEX)));

        /* Find the range (in bytes) we want to read for this index range. */
        const uint32_t nMaxBuckets = std::min(nRemaining, (nEnd - nIterator));
        if(nMaxBuckets == 5)
            debug::log(0, FUNCTION, ANSI_COLOR_BRIGHT_GREEN, "+1 extension ---------------------");

        debug::log(0, VARIABLE(nIterator),   " | ",
                      VARIABLE(nRemaining),  " | ",
                      VARIABLE(nMaxBuckets), " | ",
                      VARIABLE(nFilePos),    " | ",
                      VARIABLE(nFile),       " | ",
                      VARIABLE(nEnd),        " | ",
                      VARIABLE(nBucket),     " | ",
                      VARIABLE(nTotal),      " | "
                  );

        /* Track our current binary position, remaining buckets to read, and current bucket iterator. */
        nRemaining -= nMaxBuckets;
        nIterator  += nMaxBuckets;
    }
    while(nRemaining > 0); //continue reading into the buffer, with one loop per file adjusting to each boundary

    return true;
}


/* This is for prototyping new code. This main is accessed by building with LIVE_TESTS=1. */
int main(int argc, char** argv)
{
    config::ParseParameters(argc, argv);

    LLP::Initialize();

    config::nVerbose.store(4);
    config::mapArgs["-datadir"] = "/database";

    for(uint32_t nTotalBuckets = 0; nTotalBuckets < config::GetArg("-buckets", 1); ++nTotalBuckets)
    {
        debug::log(0, ANSI_COLOR_BRIGHT_GREEN, "Starting round ", nTotalBuckets, "----------------", ANSI_COLOR_RESET);
        runtime::sleep(1000);

        if(config::GetBoolArg("-reset", false))
        {
            std::string strPath = config::mapArgs["-datadir"] + "/testdb" + debug::safe_printstr(nTotalBuckets);

            debug::log(0, ANSI_COLOR_BRIGHT_YELLOW, "Deleting data directory ", strPath, ANSI_COLOR_RESET);
            filesystem::remove_directories(strPath);
        }

        //build our base configuration
        LLD::Config::Base BASE =
            LLD::Config::Base("testdb" + debug::safe_printstr(nTotalBuckets), LLD::FLAGS::CREATE | LLD::FLAGS::FORCE);

        //build our sector configuration
        LLD::Config::Sector SECTOR      = LLD::Config::Sector(BASE);
        SECTOR.MAX_SECTOR_FILE_STREAMS  = 16;
        SECTOR.MAX_SECTOR_BUFFER_SIZE   = 1024 * 1024 * 4; //4 MB write buffer
        SECTOR.MAX_SECTOR_CACHE_SIZE    = 256; //1 MB of cache available

        //build our hashmap configuration
        LLD::Config::Hashmap CONFIG     = LLD::Config::Hashmap(BASE);
        CONFIG.HASHMAP_TOTAL_BUCKETS    = 30 + nTotalBuckets;
        CONFIG.MAX_FILES_PER_HASHMAP    = 8;
        CONFIG.MAX_FILES_PER_INDEX      = 4;
        CONFIG.MAX_HASHMAPS             = 2;
        CONFIG.MIN_LINEAR_PROBES        = 1;
        CONFIG.MAX_LINEAR_PROBES        = 77;
        CONFIG.MAX_HASHMAP_FILE_STREAMS = 64;
        CONFIG.PRIMARY_BLOOM_HASHES     = 9;
        CONFIG.PRIMARY_BLOOM_ACCURACY   = 144;
        CONFIG.SECONDARY_BLOOM_BITS     = 13;
        CONFIG.SECONDARY_BLOOM_HASHES   = 7;
        CONFIG.QUICK_INIT               = false;


        for(uint32_t nBucket = 0; nBucket < CONFIG.HASHMAP_TOTAL_BUCKETS; ++nBucket)
        {
            read_index(nBucket, 1, CONFIG);

            continue;

            /* Calculate the number of bukcets per index file. */
            const uint32_t nTotalBuckets = (CONFIG.HASHMAP_TOTAL_BUCKETS / CONFIG.MAX_FILES_PER_HASHMAP) + 1;
            const uint32_t nMaxSize = nTotalBuckets * CONFIG.HASHMAP_KEY_ALLOCATION;

            /* Find the index of our stream based on configuration. */
            /* Calculate the file and boundaries we are on with current bucket. */
            const uint32_t nFile = (nBucket * CONFIG.MAX_FILES_PER_HASHMAP) / CONFIG.HASHMAP_TOTAL_BUCKETS;
            const uint32_t nEnd  = (((nFile) * CONFIG.HASHMAP_TOTAL_BUCKETS) / CONFIG.MAX_FILES_PER_HASHMAP);

            const uint64_t nFilePos      = (CONFIG.HASHMAP_KEY_ALLOCATION * (nBucket - nEnd - (nFile == 0 ? 0 : 1)));

            //const uint64_t nFilePos = ((nBucket) * CONFIG.HASHMAP_KEY_ALLOCATION);
            if(nFilePos == 0)
                debug::log(0, ANSI_COLOR_BRIGHT_GREEN, "------------------------", ANSI_COLOR_RESET);

            debug::log(0, VARIABLE(nBucket), " | ",
                          VARIABLE(nFile),   " | ",
                          VARIABLE(nEnd),    " | ",
                          VARIABLE(nFilePos),"/", nMaxSize
                      );
        }

        return 0;

        TestDB* bloom = new TestDB(SECTOR, CONFIG);

        //fill up database almost full
        {
            std::vector<uint1024_t> vKeys;
            for(int i = 0; i < CONFIG.HASHMAP_TOTAL_BUCKETS - 5; ++i)
                vKeys.push_back(LLC::GetRand1024());

            runtime::stopwatch swTimer;
            swTimer.start();

            uint32_t nTotal = 0;
            for(const auto& nBucket : vKeys)
            {
                ++nTotal;

                debug::log(0, ANSI_COLOR_BRIGHT_YELLOW, nTotal, ":: +++++++++++++++++++++++++++++++++", ANSI_COLOR_RESET);
                bloom->WriteKey(nBucket, nBucket);
            }
            swTimer.stop();

            uint64_t nElapsed = swTimer.ElapsedMicroseconds();
            debug::log(0, "INITIAL:: ", vKeys.size() / 1000, "k records written in ", nElapsed, " (", (1000000.0 * vKeys.size()) / nElapsed, " writes/s)");

        }

        for(int n = 0; n < config::GetArg("-tests", 1); ++n)
        {
            debug::log(0, "Generating Keys +++++++");

                std::vector<uint1024_t> vKeys;
                for(int i = 0; i < config::GetArg("-total", 10000); ++i)
                    vKeys.push_back(LLC::GetRand1024());

                debug::log(0, "------- Writing Tests...");

                runtime::stopwatch swTimer;
                swTimer.start();

                uint32_t nTotal = 0;
                for(const auto& nBucket : vKeys)
                {
                    ++nTotal;

                    debug::log(0, ANSI_COLOR_BRIGHT_YELLOW, nTotal, ":: +++++++++++++++++++++++++++++++++", ANSI_COLOR_RESET);
                    bloom->WriteKey(nBucket, nBucket);
                }
                swTimer.stop();

                uint64_t nElapsed = swTimer.ElapsedMicroseconds();
                debug::log(0, vKeys.size() / 1000, "k records written in ", nElapsed, " (", (1000000.0 * vKeys.size()) / nElapsed, " writes/s)");

                uint1024_t hashKey = 0;

                swTimer.reset();
                swTimer.start();

                debug::log(0, "------- Reading Tests...");

                nTotal = 0;
                for(const auto& nBucket : vKeys)
                {
                    ++nTotal;

                    debug::log(0, ANSI_COLOR_BRIGHT_YELLOW, nTotal, ":: +++++++++++++++++++++++++++++++++", ANSI_COLOR_RESET);
                    if(!bloom->ReadKey(nBucket, hashKey))
                        return debug::error("Failed to read ", nBucket.SubString(), " total ", nTotal);
                }
                swTimer.stop();

                nElapsed = swTimer.ElapsedMicroseconds();
                debug::log(0, vKeys.size() / 1000, "k records read in ", nElapsed, " (", (1000000.0 * vKeys.size()) / nElapsed, " read/s)");


                //test erase
                debug::log(0, "------- Erase Tests...");

                if(!bloom->EraseKey(vKeys[0]))
                    return debug::error("failed to erase ", vKeys[0].SubString());

                if(bloom->ReadKey(vKeys[0], hashKey))
                    return debug::error("Failed to erase ", vKeys[0].SubString());

        }

        delete bloom;
    }

    return 0;
}
