/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <TAO/API/include/get.h>
#include <TAO/API/include/list.h>

#include <TAO/API/users/types/users.h>

#include <TAO/API/types/exception.h>

#include <TAO/Operation/include/enum.h>

#include <TAO/Register/types/address.h>
#include <TAO/Register/types/object.h>

#include <TAO/Register/include/enum.h>
#include <TAO/Register/include/unpack.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/types/mempool.h>

#include <LLD/include/global.h>

#include <Util/include/math.h>

/* Global TAO namespace. */
namespace TAO::API
{
    /* Converts the decimals from an object into raw figures using power function */
    uint64_t GetFigures(const TAO::Register::Object& object)
    {
        return math::pow(10, GetDecimals(object));
    }


    /* Converts the decimals from an object into raw figures using power function */
    uint64_t GetFigures(const uint256_t& hashToken)
    {
        /* Check for NXS as a value. */
        if(hashToken == 0)
            return TAO::Ledger::NXS_COIN;

        /* Otherwise let's lookup our token object. */
        TAO::Register::Object tToken;
        if(!LLD::Register->ReadObject(hashToken, tToken))
            throw Exception(-13, "Object not found");

        /* Let's check that a token was passed in. */
        if(tToken.Standard() != TAO::Register::OBJECTS::TOKEN)
            throw Exception(-15, "Object is not a token");

        return math::pow(10, tToken.get<uint8_t>("decimals"));
    }


    /* Retrieves the number of decimals that applies to amounts for this token or account object.
     * If the object register passed in is a token account then we need to look at the token definition
     */
    uint8_t GetDecimals(const TAO::Register::Object& object)
    {
        /* Check the object standard. */
        switch(object.Standard())
        {
            /* Standard token contracts we can grab right from the object. */
            case TAO::Register::OBJECTS::TOKEN:
                return object.get<uint8_t>("decimals");

            /* Trust standards use NXS default values. */
            case TAO::Register::OBJECTS::TRUST:
                return TAO::Ledger::NXS_DIGITS;

            /* Account standards need to do a little looking up. */
            case TAO::Register::OBJECTS::ACCOUNT:
            {
                /* Cache our identifier. */
                const uint256_t hashIdentifier = object.get<uint256_t>("token");

                /* NXS has identifier 0, so no look up needed */
                if(hashIdentifier == 0)
                    return TAO::Ledger::NXS_DIGITS;

                /* Read the token register from the DB. */
                TAO::Register::Object object;
                if(!LLD::Register->ReadObject(hashIdentifier, object, TAO::Ledger::FLAGS::LOOKUP))
                    throw Exception(-125, "Token not found");

                return object.get<uint8_t>("decimals");
            }

            default:
                throw Exception(-36, "Unsupported type [", GetStandardName(object.Standard()), "] for command");
        }

        return 0;
    }


    /* Get the type standardized into object if applicable. */
    uint16_t GetStandardType(const TAO::Register::Object& rObject)
    {
        /* Check for a regular object type. */
        if(rObject.nType == TAO::Register::REGISTER::OBJECT)
            return 0;

        /* Reset read position. */
        rObject.nReadPos = 0;

        /* Find our leading type byte. */
        uint16_t nType;
        rObject >> nType;

        /* Cleanup our read position. */
        rObject.nReadPos = 0;

        return nType;
    }


    /* Get the sum of all debit notifications for the the specified token */
    uint64_t GetPending(const uint256_t& hashGenesis, const uint256_t& hashToken, const uint256_t& hashAccount)
    {
        /* Th return value */
        uint64_t nPending = 0;

        /* Transaction to check */
        TAO::Ledger::Transaction tx;

        /* Counter of consecutive processed events. */
        uint32_t nConsecutive = 0;

        /* The event sequence number */
        uint32_t nSequence = 0;

        /* Get the last event */
        LLD::Ledger->ReadSequence(hashGenesis, nSequence);

        /* Decrement the current sequence number to get the last event sequence number */
        --nSequence;

        /* Look back through all events to find those that are not yet processed. */
        while(LLD::Ledger->ReadEvent(hashGenesis, nSequence, tx))
        {
            /* Check to see if we have 100 (or the user configured amount) consecutive processed events.  If we do then we
               assume all prior events are also processed.  This saves us having to scan the entire chain of events */
            if(nConsecutive >= config::GetArg("-eventsdepth", 100))
                break;

            /* Loop through transaction contracts. */
            uint32_t nContracts = tx.Size();
            for(uint32_t nContract = 0; nContract < nContracts; ++nContract)
            {
                /* The proof to check for this contract */
                TAO::Register::Address hashProof;

                /* Reset the op stream */
                tx[nContract].Reset();

                /* The operation */
                uint8_t nOp;
                tx[nContract] >> nOp;

                /* Check for that the debit is meant for us. */
                if(nOp == TAO::Operation::OP::DEBIT)
                {
                    /* Get the source address which is the proof for the debit */
                    tx[nContract] >> hashProof;

                    /* Get the recipient account */
                    TAO::Register::Address hashTo;
                    tx[nContract] >> hashTo;

                    /* Retrieve the account. */
                    TAO::Register::Object account;
                    if(!LLD::Register->ReadState(hashTo, account))
                        continue;

                    /* Parse the object register. */
                    if(!account.Parse())
                        continue;

                    /* Check that this is an account */
                    if(account.Base() != TAO::Register::OBJECTS::ACCOUNT )
                        continue;

                    /* Get the token address */
                    TAO::Register::Address token = account.get<uint256_t>("token");

                    /* Check the account token matches the one passed in*/
                    if(token != hashToken)
                        continue;

                    /* Check owner that we are the owner of the recipient account  */
                    if(account.hashOwner != hashGenesis)
                        continue;

                    /* Check to see if we have already credited this debit. NOTE we do this before checking whether the account
                       for this event matches the account we are getting the pending balance for, as we are making the
                       assumption that if the last X number of events have been processed then there are no others pending
                       for any account*/
                    if(LLD::Ledger->HasProof(hashProof, tx.GetHash(), nContract, TAO::Ledger::FLAGS::MEMPOOL))
                    {
                        nConsecutive++;
                        continue;
                    }

                    /* Check the account filter */
                    if(hashAccount != 0 && hashAccount != hashTo)
                        continue;

                }
                else if(hashToken == 0 // only include coinbase for NXS token
                    && hashAccount == 0 // only include coinbase if no account is specified
                    && nOp == TAO::Operation::OP::COINBASE)
                {
                    /* Unpack the miners genesis from the contract */
                    if(!TAO::Register::Unpack(tx[nContract], hashProof))
                        continue;

                    /* Check that it is meant for our sig chain */
                    if(hashGenesis != hashProof)
                        continue;

                    /* Check to see if we have already credited this coinbase. */
                    if(LLD::Ledger->HasProof(hashProof, tx.GetHash(), nContract, TAO::Ledger::FLAGS::MEMPOOL))
                    {
                        nConsecutive++;
                        continue;
                    }
                }
                else
                    continue;

                /* Check that this notification hasn't been suppressed */
                uint64_t nTimeout = 0;
                if(LLD::Local->ReadSuppressNotification(tx.GetHash(), nContract, nTimeout) && nTimeout > runtime::unifiedtimestamp())
                    continue;

                /* Get the amount */
                uint64_t nAmount = 0;
                TAO::Register::Unpack(tx[nContract], nAmount);

                /* Add it onto our pending amount */
                nPending += nAmount;

                /* Reset the consecutive counter since this has not been processed */
                nConsecutive = 0;
            }

            /* Iterate the sequence id backwards. */
            --nSequence;
        }

        /* Next we need to include mature coinbase transactions.  We can skip this if a token as been specified as coinbase
           only apply to NXS accounts.  We can also skip if an account has been specified as coinbases can be credited to
           any account */
        if(hashToken == 0 && hashAccount == 0)
        {
            /* Get the last transaction. */
            uint512_t hashLast = 0;
            LLD::Ledger->ReadLast(hashGenesis, hashLast, TAO::Ledger::FLAGS::MEMPOOL);

            /* Get the mature coinbase transactions */
            std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> vContracts;
            Users::get_coinbases(hashGenesis, hashLast, vContracts);

            /* Iterate through all un-credited debits and see whether they apply to this token/account */
            for(const auto& contract : vContracts)
            {
                /* Get a reference to the contract */
                const TAO::Operation::Contract& refContract = std::get<0>(contract);

                /* Reset the contract operation stream. */
                refContract.Reset();

                /* Get the opcode. */
                uint8_t OPERATION;
                refContract >> OPERATION;

                /* Get the genesis hash */
                TAO::Register::Address hashFrom;
                refContract >> hashFrom;

                /* Get the amount */
                uint64_t nAmount = 0;
                refContract >> nAmount;

                /* Check that this notification hasn't been suppressed */
                uint64_t nTimeout = 0;
                if(LLD::Local->ReadSuppressNotification(refContract.Hash(), std::get<1>(contract), nTimeout) && nTimeout > runtime::unifiedtimestamp())
                    continue;

                /* Add this to the pending amount */
                nPending += nAmount;
            }
        }


        /* Now we need to look at tokenized debits that are coming in.  We can skip this if an account filter has been passed
           in as tokenized debits are not made to a specific account. */
        if(hashAccount == 0)
        {
            std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> vContracts;
            Users::get_tokenized_debits(hashGenesis, vContracts);

            /* Iterate through all un-credited debits and see whether they apply to this token/account */
            for(const auto& contract : vContracts)
            {
                /* Get a reference to the contract */
                const TAO::Operation::Contract& refContract = std::get<0>(contract);

                /* Check that this notification hasn't been suppressed */
                uint64_t nTimeout = 0;
                if(LLD::Local->ReadSuppressNotification(refContract.Hash(), std::get<1>(contract), nTimeout) && nTimeout > runtime::unifiedtimestamp())
                    continue;

                /* Reset the contract operation stream. */
                refContract.Reset();

                /* Get the opcode. */
                uint8_t OPERATION;
                refContract >> OPERATION;

                /* Get the token/account we are debiting from */
                TAO::Register::Address hashFrom;
                refContract >> hashFrom;

                /* REtrieve the account/token the debit was from */
                TAO::Register::Object from;
                if(!LLD::Register->ReadState(hashFrom, from))
                    continue;

                /* Parse the object register. */
                if(!from.Parse())
                    continue;

                /* Check the token type */
                if(from.get<uint256_t>("token") != hashToken)
                    continue;

                /* Retrieve the proof account so we can work out the partial claim amount */
                TAO::Register::Address hashProof = std::get<2>(contract);

                /* Read the object register, which is the proof account . */
                TAO::Register::Object account;
                if(!LLD::Register->ReadState(hashProof, account, TAO::Ledger::FLAGS::MEMPOOL))
                    continue;

                /* Parse the object register. */
                if(!account.Parse())
                    continue;

                /* Check that this is an account */
                if(account.Standard() != TAO::Register::OBJECTS::ACCOUNT )
                    continue;

                /* Get the token address */
                TAO::Register::Address hashProofToken = account.get<uint256_t>("token");

                /* Read the token register. */
                TAO::Register::Object token;
                if(!LLD::Register->ReadState(hashProofToken, token, TAO::Ledger::FLAGS::MEMPOOL))
                    continue;

                /* Parse the object register. */
                if(!token.Parse())
                    continue;

                /* Get the token supply so that we an determine our share */
                uint64_t nSupply = token.get<uint64_t>("supply");

                /* Get the balance of our token account */
                uint64_t nBalance = account.get<uint64_t>("balance");

                /* Get the amount from the debit contract*/
                uint64_t nAmount = 0;
                TAO::Register::Unpack(refContract, nAmount);

                /* Calculate the partial debit amount that this token holder is entitled to. */
                uint64_t nPartial = (nAmount * nBalance) / nSupply;

                /* Add this to our pending balance */
                nPending += nPartial;
            }
        }

        return nPending;
    }


    /* Get the sum of all debit transactions in the mempool for the the specified token */
    uint64_t GetUnconfirmed(const uint256_t& hashGenesis, const uint256_t& hashToken, bool fOutgoing, const uint256_t& hashAccount)
    {
        /* The return value */
        uint64_t nUnconfirmed = 0;

        /* Get all transactions in the mempool */
        std::vector<uint512_t> vMempool;
        if(TAO::Ledger::mempool.List(vMempool))
        {
            /* Loop through the list of transactions. */
            for(const auto& hash : vMempool)
            {
                /* Get the transaction from the memory pool. */
                TAO::Ledger::Transaction tx;
                if(!TAO::Ledger::mempool.Get(hash, tx))
                    continue;

                /* Loop through transaction contracts. */
                uint32_t nContracts = tx.Size();
                for(uint32_t nContract = 0; nContract < nContracts; ++nContract)
                {
                    /* Reset the op stream */
                    tx[nContract].Reset();

                    /* The operation */
                    uint8_t nOp;
                    tx[nContract] >> nOp;

                    /* Check for that the debit is meant for us. */
                    if(nOp == TAO::Operation::OP::DEBIT)
                    {
                        /* Get the source address which is the proof for the debit */
                        TAO::Register::Address hashFrom;
                        tx[nContract] >> hashFrom;

                        /* Get the recipient account */
                        TAO::Register::Address hashTo;
                        tx[nContract] >> hashTo;

                        /* Get the amount */
                        uint64_t nAmount = 0;
                        tx[nContract] >> nAmount;

                        /* If caller wants outgoing transactions then check that we made the transaction */
                        if(fOutgoing)
                        {
                            /* Check we made the transaction */
                            if(tx.hashGenesis != hashGenesis)
                                continue;

                            /* Check the account filter based on the originating account*/
                            if(hashAccount != 0 && hashAccount != hashFrom)
                                continue;

                            /* Retrieve the account. */
                            TAO::Register::Object account;
                            if(!LLD::Register->ReadState(hashFrom, account))
                                continue;

                            /* Parse the object register. */
                            if(!account.Parse())
                                continue;

                            /* Check that this is an account */
                            if(account.Base() != TAO::Register::OBJECTS::ACCOUNT )
                                continue;

                            /* Get the token address */
                            TAO::Register::Address token = account.get<uint256_t>("token");

                            /* Check the account token matches the one passed in*/
                            if(token != hashToken)
                                continue;
                        }
                        else
                        {
                            /* Check the account filter */
                            if(hashAccount != 0 && hashAccount != hashTo)
                                continue;

                            /* Retrieve the account. */
                            TAO::Register::Object account;
                            if(!LLD::Register->ReadState(hashTo, account))
                                continue;

                            /* Parse the object register. */
                            if(!account.Parse())
                                continue;

                            /* Check that this is an account */
                            if(account.Base() != TAO::Register::OBJECTS::ACCOUNT )
                                continue;

                            /* Get the token address */
                            TAO::Register::Address token = account.get<uint256_t>("token");

                            /* Check the account token matches the one passed in*/
                            if(token != hashToken)
                                continue;

                            /* Check owner that we are the owner of the recipient account  */
                            if(account.hashOwner != hashGenesis)
                                continue;
                        }

                        /* Add this amount to our total */
                        nUnconfirmed += nAmount;

                    }
                    /* Check for that the credits we made . */
                    else if(!fOutgoing && nOp == TAO::Operation::OP::CREDIT)
                    {
                        /* For credits first make sure we made the transaction */
                        if(tx.hashGenesis != hashGenesis)
                            continue;

                        /* Extract the transaction from contract. */
                        uint512_t hashTx = 0;
                        tx[nContract] >> hashTx;

                        /* Extract the contract-id. */
                        uint32_t nID = 0;
                        tx[nContract] >> nID;

                        /* Get the account address. */
                        TAO::Register::Address hashTo;
                        tx[nContract] >> hashTo;

                        /* Get the proof address. */
                        TAO::Register::Address hashProof;
                        tx[nContract] >> hashProof;

                        /* Get the credit amount. */
                        uint64_t nCredit = 0;
                        tx[nContract] >> nCredit;

                        /* Check the account filter */
                        if(hashAccount != 0 && hashAccount != hashTo)
                            continue;

                        /* Retrieve the account. */
                        TAO::Register::Object account;
                        if(!LLD::Register->ReadState(hashTo, account))
                            continue;

                        /* Parse the object register. */
                        if(!account.Parse())
                            continue;

                        /* Check that this is an account */
                        if(account.Base() != TAO::Register::OBJECTS::ACCOUNT )
                            continue;

                        /* Get the token address */
                        TAO::Register::Address token = account.get<uint256_t>("token");

                        /* Check the account token matches the one passed in*/
                        if(token != hashToken)
                            continue;

                        /* Check owner that we are the owner of the recipient account  */
                        if(account.hashOwner != hashGenesis)
                            continue;

                        /* Add this amount to our total */
                        nUnconfirmed += nCredit;
                    }
                    /* Check for outgoing OP::LEGACY */
                    else if(fOutgoing && nOp == TAO::Operation::OP::LEGACY)
                    {
                        /* Check the token filter is 0 as OP::LEGACY are only for NXS accounts*/
                        if(hashToken != 0)
                            continue;

                        /* Get the source address which is the proof for the debit */
                        TAO::Register::Address hashFrom;
                        tx[nContract] >> hashFrom;

                        /* Get the amount */
                        uint64_t nAmount = 0;
                        tx[nContract] >> nAmount;

                        /* Check we made the transaction */
                        if(tx.hashGenesis != hashGenesis)
                            continue;

                        /* Check the account filter based on the originating account*/
                        if(hashAccount != 0 && hashAccount != hashFrom)
                            continue;

                        /* Add this amount to our total */
                        nUnconfirmed += nAmount;
                    }
                }
            }
        }

        return nUnconfirmed;
    }


    /*  Get the outstanding coinbases. */
    //XXX: we are iterating the entire sigchain for each of these items, this needs to be optimized
    uint64_t GetImmature(const uint256_t& hashGenesis)
    {
        /* Return amount */
        uint64_t nImmature = 0;

        /* Get the last transaction. */
        uint512_t hashLast = 0;
        if(LLD::Ledger->ReadLast(hashGenesis, hashLast, TAO::Ledger::FLAGS::MEMPOOL))
        {
            /* Reverse iterate until genesis (newest to oldest). */
            while(hashLast != 0)
            {
                /* Get the transaction from disk. */
                TAO::Ledger::Transaction tx;
                if(!LLD::Ledger->ReadTx(hashLast, tx, TAO::Ledger::FLAGS::MEMPOOL))
                {
                    /* In client mode it is possible to not have the full sig chain if it is still being downloaded asynchronously.*/
                    if(config::fClient.load())
                        break;
                    else
                        throw Exception(-108, "Failed to read transaction");
                }

                /* Skip this transaction if it is mature. */
                if(LLD::Ledger->ReadMature(hashLast))
                {
                    /* Set the next last. */
                    hashLast = !tx.IsFirst() ? tx.hashPrevTx : 0;
                    continue;
                }

                /* Loop through all contracts and check that it is a coinbase. */
                uint32_t nContracts = tx.Size();
                for(uint32_t nContract = 0; nContract < nContracts; ++nContract)
                {
                    uint64_t nAmount = 0;

                    /* The operation */
                    uint8_t nOp = 0;

                    /* Reset the contract operation stream */
                    tx[nContract].Reset();

                    /* Get the operation */
                    tx[nContract] >> nOp;

                    /* Check for coinbase or coinstake opcode */
                    if(nOp == Operation::OP::COINBASE)
                    {
                        /* Get the amount */
                        TAO::Register::Unpack(tx[nContract], nAmount);

                        /* Get the genesis of the coinbase recipient*/
                        TAO::Register::Address hashRecipient;
                        TAO::Register::Unpack(tx[nContract], hashRecipient);

                        /* Check that the contract was for us */
                        if(hashRecipient == hashGenesis)
                            /* Add it to our return values */
                            nImmature += nAmount;

                    }
                }

                /* Set the next last. */
                hashLast = !tx.IsFirst() ? tx.hashPrevTx : 0;
            }
        }

        return nImmature;
    }


    /* Calculates the percentage of tokens owned from the total supply. */
    //XXX: good god, more O(n^2) algorithms, delete and/or refactor this code
    double GetTokenOwnership(const TAO::Register::Address& hashToken, const uint256_t& hashGenesis)
    {
        /* Find all token accounts owned by the caller for the token */
        std::vector<TAO::Register::Address> vAccounts;
        ListAccounts(hashGenesis, vAccounts, true, false);

        /* The balance of tokens owned for this asset */
        uint64_t nBalance = 0;
        for(const auto& hashAccount : vAccounts)
        {
            /* Make sure it is an account or the token itself (in case not all supply has been distributed)*/
            if(!hashAccount.IsAccount() && !hashAccount.IsToken())
                continue;

            /* Get the account from the register DB. */
            TAO::Register::Object object;
            if(!LLD::Register->ReadState(hashAccount, object, TAO::Ledger::FLAGS::MEMPOOL))
                throw Exception(-13, "Object not found");

            /* Check that this is a non-standard object type so that we can parse it and check the type*/
            if(object.nType != TAO::Register::REGISTER::OBJECT)
                continue;

            /* parse object so that the data fields can be accessed */
            if(!object.Parse())
                throw Exception(-36, "Failed to parse object register");

            /* Check that this is an account or token */
            if(object.Base() != TAO::Register::OBJECTS::ACCOUNT)
                continue;

            /* Check the token*/
            if(object.get<uint256_t>("token") != hashToken)
                continue;

            /* Get the balance */
            nBalance += object.get<uint64_t>("balance");
        }

        /* If we have a balance > 0 then we have some ownership, so work out the % */
        if(nBalance > 0)
        {
            /* Retrieve the token itself so we can get the supply */
            TAO::Register::Object token;
            if(!LLD::Register->ReadState(hashToken, token, TAO::Ledger::FLAGS::MEMPOOL))
                throw Exception(-125, "Token not found");

            /* Parse the object register. */
            if(!token.Parse())
                throw Exception(-14, "Object failed to parse");

            /* Get the total supply */
            uint64_t nSupply = token.get<uint64_t>("supply");

            /* Calculate the ownership % */
            return (double)nBalance / (double)nSupply * 100.0;
        }
        else
        {
            return 0.0;
        }

    }


    /* Reads a batch of states registers from the Register DB */
    bool GetRegisters(const std::vector<TAO::Register::Address>& vAddresses,
                      std::vector<std::pair<TAO::Register::Address, TAO::Register::State>>& vStates)
    {
        for(const auto& hashRegister : vAddresses)
        {
            /* Get the state from the register DB. */
            TAO::Register::State state;
            if(!LLD::Register->ReadState(hashRegister, state, TAO::Ledger::FLAGS::MEMPOOL))
                throw Exception(-104, "Object not found");

            vStates.push_back(std::make_pair(hashRegister, state));
        }

        /* Now sort the states based on the creation time */
        std::sort(vStates.begin(), vStates.end(),
            [](const std::pair<TAO::Register::Address, TAO::Register::State> &a,
            const std::pair<TAO::Register::Address, TAO::Register::State> &b)
            {
                return ( a.second.nCreated < b.second.nCreated );
            });

        return vStates.size() > 0;
    }


    //XXX: this method is only used once, and is O(n^2) complexity (scans sigchain twice), sheesh. We need to delete this and work on O(1) version
    /* Searches the sig chain for the first account for the given token type */
    bool GetAccountByToken(const uint256_t& hashGenesis, const uint256_t& hashToken, TAO::Register::Address& hashAccount)
    {
        /* We search for the first account of the specified token type.  NOTE that the owner my not
           have an account for the token at this stage, in which case we can just skip the notification
           for now until they do have one */

        /* Get the list of registers owned by this sig chain */
        std::vector<TAO::Register::Address> vAccounts;
        if(!ListAccounts(hashGenesis, vAccounts, false, false))
            throw Exception(-74, "No registers found");

        /* Read all the registers to that they are sorted by creation time */
        std::vector<std::pair<TAO::Register::Address, TAO::Register::State>> vRegisters;
        GetRegisters(vAccounts, vRegisters);

        /* Iterate all registers to find the first for the token being credited */
        for(const auto& state : vRegisters)
        {
            /* Double check that it is an object before we cast it */
            if(state.second.nType != TAO::Register::REGISTER::OBJECT)
                continue;

            /* Cast the state to an Object register */
            TAO::Register::Object object(state.second);

            /* Check that this is a non-standard object type so that we can parse it and check the type*/
            if(object.nType != TAO::Register::REGISTER::OBJECT)
                continue;

            /* parse object so that the data fields can be accessed */
            if(!object.Parse())
                throw Exception(-36, "Failed to parse object register");

            /* Check that this is an account */
            uint8_t nStandard = object.Standard();
            if(nStandard != TAO::Register::OBJECTS::ACCOUNT)
                continue;

            /* Check the account is for the specified token */
            if(object.get<uint256_t>("token") != hashToken)
                continue;
            else
            {
                /* Stop on the first one we find */
                hashAccount = state.first;
                return true;
            }
        }

        return false;
    }


    /* Returns a type string for the register type */
    std::string GetRegisterName(const uint8_t nType)
    {
        /* Switch based on register type. */
        switch(nType)
        {
            /* State type is READONLY. */
            case TAO::Register::REGISTER::READONLY:
                return "READONLY";

            /* State type is APPEND. */
            case TAO::Register::REGISTER::APPEND:
                return "APPEND";

            /* State type is RAW. */
            case TAO::Register::REGISTER::RAW:
                return "RAW";

            /* State type is OBJECT. */
            case TAO::Register::REGISTER::OBJECT:
                return "OBJECT";

            /* State type is SYSTEM. */
            case TAO::Register::REGISTER::SYSTEM:
                return "SYSTEM";
        }

        return "UNKNOWN";
    }


    /* Returns a type string for the register object type */
    std::string GetStandardName(const uint8_t nType)
    {
        /* Switch based on standard type. */
        switch(nType)
        {
            /* Non Standard types are NONSTANDARD. */
            case TAO::Register::OBJECTS::NONSTANDARD:
                return "NONSTANDARD";
                //strObjectType = "REGISTER";

            /* Account standard types are ACCOUNT. */
            case TAO::Register::OBJECTS::ACCOUNT:
                return "ACCOUNT";

            /* Name standard types are NAME. */
            case TAO::Register::OBJECTS::NAME:
                return "NAME";

            /* Namespace standard types are NAMESPACE. */
            case TAO::Register::OBJECTS::NAMESPACE:
                return "NAMESPACE";

            /* Token standard types are TOKEN. */
            case TAO::Register::OBJECTS::TOKEN:
                return "TOKEN";

            /* Trust standard types are TRUST. */
            case TAO::Register::OBJECTS::TRUST:
                return "TRUST";

            /* Crypto standard types are CRYPTO. */
            case TAO::Register::OBJECTS::CRYPTO:
                return "CRYPTO";
        }

        return "UNKNOWN";
    }
} // End TAO namespace
