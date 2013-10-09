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

#ifndef RIPPLE_BASICS_LOGJOURNAL_H_INCLUDED
#define RIPPLE_BASICS_LOGJOURNAL_H_INCLUDED

/** Adapter that exports ripple::Log functionality as a Journal::Sink. */
class LogJournal
{
public:
    /** Convert the Journal::Severity to a LogSeverity. */
    static LogSeverity convertSeverity (Journal::Severity severity);

    //--------------------------------------------------------------------------

    /** A Journal::Sink that writes to the LogPartition with a given Key. */
    template <class Key>
    class PartitionSink : public Journal::Sink
    {
    public:
        PartitionSink ()
            : m_partition (LogPartition::get <Key> ())
            , m_severity (Journal::kLowestSeverity)
            , m_to_console (false)
        {
        }

        void write (Journal::Severity severity, std::string const& text)
        {
            std::string output;
            LogSeverity const logSeverity (convertSeverity (severity));
            LogSink::get()->format (output, text, logSeverity,
                m_partition.getName());
            LogSink::get()->write (output, logSeverity);
        #if BEAST_MSVC
            if (m_to_console && beast_isRunningUnderDebugger ())
                Logger::outputDebugString (output.c_str());
        #endif
        }

        bool active (Journal::Severity severity)
        {
            return m_partition.doLog (convertSeverity (severity));
        }

        bool console()
        {
            return m_to_console;
        }

        void set_severity (Journal::Severity severity)
        {
            LogSeverity const logSeverity (convertSeverity (severity));
            m_partition.setMinimumSeverity (logSeverity);
        }

        void set_console (bool to_console)
        {
            m_to_console = to_console;
        }

    private:
        LogPartition& m_partition;
        Journal::Severity m_severity;
        bool m_to_console;
    };

    //--------------------------------------------------------------------------

    /** Returns a Journal outputting through the LogPartition for the Key. */ 
    template <class Key>
    static Journal get ()
    {
        return Journal (*SharedSingleton <PartitionSink <Key> >::get (
            SingletonLifetime::neverDestroyed));
    }
};

#endif
