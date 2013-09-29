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


SETUP_LOGN (WSConnection, "WSConnection")

static std::string trimIP(const std::string& ip)
{ // Make sure there's no port
    size_t pos = ip.find(':');
    return (pos == std::string::npos) ? ip : ip.substr(0, pos - 1);
}

//------------------------------------------------------------------------------

WSConnection::WSConnection (InfoSub::Source& source, bool isPublic,
    std::string const& remoteIP, boost::asio::io_service& io_service)
    : InfoSub (source)
    , m_isPublic (isPublic)
    , m_remoteIP (remoteIP)
    , m_receiveQueueMutex (this, "WSConnection", __FILE__, __LINE__)
    , m_netOPs (getApp ().getOPs ())
    , m_loadSource (m_remoteIP, trimIP(m_remoteIP))
    , m_pingTimer (io_service)
    , m_sentPing (false)
    , m_receiveQueueRunning (false)
    , m_isDead (false)
    , m_io_service (io_service)
{
    WriteLog (lsDEBUG, WSConnection) << "Websocket connection from " << m_remoteIP;
}

WSConnection::~WSConnection ()
{
}

void WSConnection::onPong (const std::string&)
{
    m_sentPing = false;
}

void WSConnection::rcvMessage (message_ptr msg, bool& msgRejected, bool& runQueue)
{
    ScopedLockType sl (m_receiveQueueMutex, __FILE__, __LINE__);

    if (m_isDead)
    {
        msgRejected = false;
        runQueue = false;
        return;
    }

    if (m_isDead || (m_receiveQueue.size () >= 1000))
    {
        msgRejected = !m_isDead;
        runQueue = false;
    }
    else
    {
        msgRejected = false;
        m_receiveQueue.push_back (msg);

        if (m_receiveQueueRunning)
            runQueue = false;
        else
        {
            runQueue = true;
            m_receiveQueueRunning = true;
        }
    }
}

WSConnection::message_ptr WSConnection::getMessage ()
{
    ScopedLockType sl (m_receiveQueueMutex, __FILE__, __LINE__);

    if (m_isDead || m_receiveQueue.empty ())
    {
        m_receiveQueueRunning = false;
        return message_ptr ();
    }

    message_ptr m = m_receiveQueue.front ();
    m_receiveQueue.pop_front ();
    return m;
}

void WSConnection::returnMessage (message_ptr ptr)
{
    ScopedLockType sl (m_receiveQueueMutex, __FILE__, __LINE__);

    if (!m_isDead)
        m_receiveQueue.push_front(ptr);
}

Json::Value WSConnection::invokeCommand (Json::Value& jvRequest)
{
    // VFALCO TODO Make LoadManager a ctor argument
    if (getApp().getLoadManager ().shouldCutoff (m_loadSource))
    {
        disconnect ();
        return rpcError (rpcSLOW_DOWN);
    }

    // Requests without "command" are invalid.
    //
    if (!jvRequest.isMember ("command"))
    {
        Json::Value jvResult (Json::objectValue);

        jvResult["type"]    = "response";
        jvResult["status"]  = "error";
        jvResult["error"]   = "missingCommand";
        jvResult["request"] = jvRequest;

        if (jvRequest.isMember ("id"))
        {
            jvResult["id"]  = jvRequest["id"];
        }

        getApp().getLoadManager ().applyLoadCharge (m_loadSource, LT_RPCInvalid);

        return jvResult;
    }

    LoadType loadType = LT_RPCReference;
    RPCHandler  mRPCHandler (&this->m_netOPs, boost::dynamic_pointer_cast<InfoSub> (this->shared_from_this ()));
    Json::Value jvResult (Json::objectValue);

    Config::Role const role = m_isPublic
            ? Config::GUEST     // Don't check on the public interface.
            : getConfig ().getAdminRole (jvRequest, m_remoteIP);
        
    if (Config::FORBID == role)
    {
        jvResult["result"]  = rpcError (rpcFORBIDDEN);
    }
    else
    {
        jvResult["result"] = mRPCHandler.doCommand (jvRequest, role, &loadType);
    }

    // Debit/credit the load and see if we should include a warning.
    //
    if (getApp().getLoadManager ().applyLoadCharge (m_loadSource, loadType) &&
        getApp().getLoadManager ().shouldWarn (m_loadSource))
    {
        jvResult["warning"] = "load";
    }

    // Currently we will simply unwrap errors returned by the RPC
    // API, in the future maybe we can make the responses
    // consistent.
    //
    // Regularize result. This is duplicate code.
    if (jvResult["result"].isMember ("error"))
    {
        jvResult            = jvResult["result"];
        jvResult["status"]  = "error";
        jvResult["request"] = jvRequest;

    }
    else
    {
        jvResult["status"]  = "success";
    }

    if (jvRequest.isMember ("id"))
    {
        jvResult["id"]      = jvRequest["id"];
    }

    jvResult["type"]        = "response";

    return jvResult;
}
