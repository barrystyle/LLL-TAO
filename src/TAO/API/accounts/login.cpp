/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLC/include/random.h>

#include <LLD/include/global.h>

#include <TAO/API/include/accounts.h>

#include <TAO/Ledger/types/sigchain.h>
#include <TAO/Ledger/types/mempool.h>

/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        //TODO: have the authorization system build a SHA256 hash and salt on the client side as the AUTH hash.

        /* Login to a user account. */
        json::json Accounts::Login(const json::json& params, bool fHelp)
        {
            /* JSON return value. */
            json::json ret;

            /* For sessionless API use the active sig chain which is stored in session 0 */

            /* Check for username parameter. */
            if(params.find("username") == params.end())
                throw APIException(-23, "Missing Username");

            /* Check for password parameter. */
            if(params.find("password") == params.end())
                throw APIException(-24, "Missing Password");

            /* Create the sigchain. */
            TAO::Ledger::SignatureChain user(params["username"].get<std::string>().c_str(), params["password"].get<std::string>().c_str());

            /* Get the genesis ID. */
            uint256_t hashGenesis = user.Genesis();

            /* Check for duplicates in ledger db. */
            TAO::Ledger::Transaction tx;
            if(!LLD::legDB->HasGenesis(hashGenesis))
            {
                /* Check the memory pool (TODO: Paul maybe you can think of a more efficient way to solve this chicken and egg). */
                if(!TAO::Ledger::mempool.Has(hashGenesis))
                {
                    throw APIException(-26, "Account doesn't exists");
                }
            }

            /* Check the sessions. */
            for(auto session = mapSessions.begin(); session != mapSessions.end(); ++ session)
            {
                if(hashGenesis == session->second.Genesis())
                {
                    /* already logged in */
                    throw APIException(-26, "User already logged in");
                }
            }

            /* Set the return value. */
            /* For sessionless API use the active sig chain which is stored in session 0 */
            uint64_t nSession = config::fAPISessions ? LLC::GetRand() : 0;
            ret["genesis"] = hashGenesis.ToString();
            if( config::fAPISessions)
                ret["session"] = nSession;

            /* Setup the account. */
            mapSessions[nSession] = user;
            strActivePIN = "";

            return ret;
        }
    }
}
