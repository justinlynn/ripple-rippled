//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================


#ifndef RIPPLE_VALIDATORS_SOURCE_H_INCLUDED
#define RIPPLE_VALIDATORS_SOURCE_H_INCLUDED

namespace ripple {
namespace Validators {

/** A source of validator descriptors. */
class Source
{
public:
    /** A Source's descriptor for a Validator. */
    struct Info
    {
        /** The unique key for this validator. */
        RipplePublicKey publicKey;

        /** Optional human readable comment describing the validator. */
        String label;
    };

    /** Destroy the Source.
        This can be called from any thread. If the Source is busy
        fetching, the destructor must block until the operation is either
        canceled or complete.
    */
    virtual ~Source () { }

    virtual String name () = 0;

    virtual String uniqueID () = 0;

    virtual String createParam () = 0;

    /** Fetch the most recent list from the Source.
        If possible, the Source should periodically poll the
        CancelCallback, and abort the operation if shouldCancel
        returns `true`.
        This call will block.
    */
    struct Result
    {
        Result ();
        void swapWith (Result& other);

        bool success;
        String message;
        Time expirationTime;
        Array <Info> list;
    };
    
    virtual Result fetch (CancelCallback& callback, Journal journal) = 0;
};

}
}

#endif
