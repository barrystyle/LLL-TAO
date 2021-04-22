/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLD/include/global.h>

#include <TAO/API/include/build.h>
#include <TAO/API/include/global.h>
#include <TAO/API/include/get.h>

#include <TAO/Operation/include/enum.h>

#include <TAO/Ledger/include/create.h>
#include <TAO/Ledger/types/mempool.h>


/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /* Burn tokens. This debits the account to send the coins back to the token address, but does so with a contract condition
           that always evaluates to false.  Hence the debit can never be credited, burning the tokens. */
        json::json Tokens::Burn(const json::json& params, bool fHelp)
        {
            /* The sending account or token. */
            TAO::Register::Address hashFrom;

            /* If name is provided then use this to deduce the register address,
             * otherwise try to find the raw hex encoded address. */
            if(params.find("name") != params.end() && !params["name"].get<std::string>().empty())
                hashFrom = Names::ResolveAddress(params, params["name"].get<std::string>());
            else if(params.find("address") != params.end())
                hashFrom.SetBase58(params["address"].get<std::string>());
            else
                throw APIException(-33, "Missing name / address");

            /* Get the token / account object. */
            TAO::Register::Object object;
            if(!LLD::Register->ReadState(hashFrom, object, TAO::Ledger::FLAGS::MEMPOOL))
                throw APIException(-122, "Token/account not found");

            /* Parse the object register. */
            if(!object.Parse())
                throw APIException(-14, "Object failed to parse");

            /* Get the object standard. */
            uint8_t nStandard = object.Standard(), nDecimals = 0; //XXX: maybe we want a short for decimals? are we limited to 8 bits?

            /* Balance of account/token being debited */
            uint64_t nCurrentBalance = 0;

            /* Check the object standard. */
            uint256_t hashToken = 0;
            if(nStandard == TAO::Register::OBJECTS::ACCOUNT)
            {
                nCurrentBalance = object.get<uint64_t>("balance");
                nDecimals = GetDecimals(object);
                hashToken = object.get<uint256_t>("token");
            }
            else
            {
                throw APIException(-65, "Object is not an account");
            }

            /* Check for amount parameter. */
            if(params.find("amount") == params.end())
                throw APIException(-46, "Missing amount");

            /* Get the amount to debit. */
            uint64_t nAmount = std::stod(params["amount"].get<std::string>()) * pow(10, nDecimals);

            /* Check the amount is not too small once converted by the token Decimals */
            if(nAmount == 0)
                throw APIException(-68, "Amount too small");

            /* Check they have the required funds */
            if(nAmount > nCurrentBalance)
                throw APIException(-69, "Insufficient funds");

            /* The optional payment reference */
            uint64_t nReference = 0;
            if(params.find("reference") != params.end())
                nReference = stoull(params["reference"].get<std::string>());

            /* Submit the payload object. */
            std::vector<TAO::Operation::Contract> vContracts(1);
            vContracts[0] << (uint8_t)TAO::Operation::OP::DEBIT << hashFrom << hashToken << nAmount << nReference;

            /* Add the burn conditions.  This is a simple condition that will never evaluate to true */
            vContracts[0] <= uint8_t(TAO::Operation::OP::TYPES::UINT16_T) <= uint16_t(108+105+102+101);
            vContracts[0] <= uint8_t(TAO::Operation::OP::EQUALS);
            vContracts[0] <= uint8_t(TAO::Operation::OP::TYPES::UINT16_T) <= uint16_t(42);

            /* Build a JSON response object. */
            json::json ret;
            ret["txid"] = BuildAndAccept(params, vContracts).ToString();

            return ret;
        }
    }
}
