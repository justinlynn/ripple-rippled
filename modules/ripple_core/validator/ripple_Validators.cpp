//------------------------------------------------------------------------------
/*
    Copyright (c) 2011-2013, OpenCoin, Inc.
*/
//==============================================================================

/*

Two important questions:

- Are there any validators in my ChosenValidators that I dont want
  * For example, they have dropped off all the trusted lists

- Do I have enough?

*/

class ValidatorsImp
    : public Validators
    , private ThreadWithCallQueue::EntryPoints
    , private DeadlineTimer::Listener
{
public:
    // Tunable constants
    enum
    {
        // We will fetch a source at this interval
         hoursBetweenFetches = 24

        ,secondsBetweenFetches = hoursBetweenFetches * 60 * 60

        // Wake up every hour to check source times
        ,timerGranularity = 60 * 60

        // This tunes the preallocated arrays
        ,expectedNumberOfResults = 1000
    };

    //--------------------------------------------------------------------------

    struct ValidatorInfoCompare
    {
        static int compareElements (ValidatorInfo const& lhs, ValidatorInfo const& rhs)
        {
            int result;

            if (lhs.publicKey < rhs.publicKey)
                result = -1;
            else if (lhs.publicKey > rhs.publicKey)
                result = 1;
            else
                result = 0;

            return result;
        }
    };

    struct SourceInfo
    {
        enum Status
        {
            statusNone,
            statusFetched,
            statusFailed,
        };

        explicit SourceInfo (Source* source_)
            : source (source_)
            , status (statusNone)
            , numberOfFailures (0)
        {
        }

        Source* source;
        Status status;
        Time whenToFetch;

        /** The number of times a fetch has failed.
        */
        int numberOfFailures;
    };

    // This is what comes back from a source
    typedef Array <SourceInfo> SourceInfoArray;

    typedef Array <ValidatorInfo> ValidatorInfoArray;

    // The result of performing a fetch
    struct FetchResult
    {
        // This is what comes back from the fetch
        ValidatorInfoArray updatedList;

        // The original list before the fetch
        SharedObjectArray <Validator> oldList;

        // The new list after the fetch
        SharedObjectArray <Validator> newList;

        // The list of validators that were added
        SharedObjectArray <Validator> addedList;

        // The list of validators that were removed
        SharedObjectArray <Validator> removedList;

        FetchResult ()
        {
            updatedList.ensureStorageAllocated (expectedNumberOfResults);
            oldList.ensureStorageAllocated (expectedNumberOfResults);
            newList.ensureStorageAllocated (expectedNumberOfResults);
            addedList.ensureStorageAllocated (expectedNumberOfResults);
            removedList.ensureStorageAllocated (expectedNumberOfResults);
        }

        void clear ()
        {
            updatedList.clearQuick ();
            oldList.clear ();
            newList.clear ();
            addedList.clear ();
            removedList.clear ();
        }
    };

    //--------------------------------------------------------------------------

    // Encapsulates the logic for creating the chosen validators.
    // This is a separate class to facilitate the unit tests.
    //
    class Logic
    {
    public:
        Logic ()
            : m_knownValidators (new ValidatorListImp)
            , m_chosenValidators (new ValidatorListImp)
        {
        }

        void addSource (Source* source)
        {
            m_sources.add (source);

            m_sourceInfo.add (SourceInfo (source));
        }

        SourceInfoArray& getSources ()
        {
            return m_sourceInfo;
        }

        void processValidatorInfo (ValidatorInfoArray& fetchResults)
        {
        }

        void sortValidatorInfo (ValidatorInfoArray& arrayToSort)
        {
            ValidatorInfoArray sorted;

            sorted.ensureStorageAllocated (arrayToSort.size ());

            for (int i = 0; i < arrayToSort.size (); ++i)
                sorted.addSorted (ValidatorInfoCompare(), arrayToSort [i]);

            arrayToSort.swapWithArray (sorted);
        }

        void fetchSource (SourceInfo& sourceInfo)
        {

        }

    private:
        OwnedArray <Source> m_sources;
        SourceInfoArray m_sourceInfo;
        ValidatorList::Ptr m_knownValidators;
        ValidatorList::Ptr m_chosenValidators;
    };

    //--------------------------------------------------------------------------

public:
    explicit ValidatorsImp (Validators::Listener* listener)
        : m_listener (listener)
        , m_thread ("Validators")
        , m_timer (this)
    {
        m_thread.start (this);
    }

    ~ValidatorsImp ()
    {
    }

    void addSource (Source* source)
    {
        m_thread.call (&ValidatorsImp::doAddSource, this, source);
    }

    void doAddSource (Source* source)
    {
        m_logic.addSource (source);
    }

    void onDeadlineTimer (DeadlineTimer&)
    {
        // This will make us fall into the idle proc as needed
        //
        m_thread.interrupt ();
    }

    // Goes through all the sources and refreshes them as needed
    //
    bool scanSources ()
    {
        bool interrupted = false;

        Time currentTime = Time::getCurrentTime ();

        ValidatorInfoArray fetchResults;
        fetchResults.ensureStorageAllocated (1000);

        SourceInfoArray sourceInfoArray (m_logic.getSources ());

        // Find a source that needs to be processed
        //
        for (int i = 0; i < sourceInfoArray.size (); ++i)
        {
            SourceInfo& sourceInfo (sourceInfoArray.getReference (i));

            // See if we need to refresh its list
            //
            if (currentTime <= sourceInfo.whenToFetch)
            {
                sourceInfo.source->fetch (fetchResults);

                currentTime = Time::getCurrentTime ();

                if (fetchResults.size () != 0)
                {
                    sourceInfo.status = SourceInfo::statusFetched;
                    sourceInfo.whenToFetch = currentTime + RelativeTime (hoursBetweenFetches * 60.0 * 60.0);

                    m_logic.processValidatorInfo (fetchResults);

                    //m_listener->onValidatorsChosen (chosenValidators);
                }
                else
                {
                    // Failed to fetch
                    // Don't update fetch time
                    sourceInfo.status = SourceInfo::statusFailed;
                    sourceInfo.numberOfFailures++;
                }

                fetchResults.clearQuick ();
            }

            interrupted = m_thread.interruptionPoint ();

            if (interrupted)
                break;
        }

        return interrupted;
    }

    void threadInit ()
    {
        m_timer.setRecurringExpiration (timerGranularity);
    }

    void threadExit ()
    {
    }

    bool threadIdle ()
    {
        bool interrupted = false;

        interrupted = scanSources ();

        return interrupted;
    }

private:
    Logic m_logic;
    Validators::Listener* const m_listener;
    ThreadWithCallQueue m_thread;
    DeadlineTimer m_timer;
};

Validators* Validators::New (Listener* listener)
{
    return new ValidatorsImp (listener);
}

//------------------------------------------------------------------------------

// Produces validators for unit tests.
class TestValidatorSource : public Validators::Source
{
public:
    TestValidatorSource (unsigned int startIndex, int numEntries)
        : m_startIndex (startIndex)
        , m_numEntries (numEntries)
    {
    }

    void fetch (Array <ValidatorInfo>& results)
    {
        for (unsigned int publicKeyIndex = m_startIndex;
             publicKeyIndex < m_startIndex + m_numEntries;
             ++publicKeyIndex)
        {
            ValidatorInfo info;

            info.publicKey = Validator::PublicKey::createFromInteger (publicKeyIndex);

            results.add (info);
        }
    }

private:
    unsigned const m_startIndex;
    int const m_numEntries;
};

//------------------------------------------------------------------------------

class ValidatorListTests : public UnitTestType <ValidatorListTests>
{
public:
    ValidatorListTests () : UnitTestType <ValidatorListTests> ("ValidatorList")
    {
    }

    void publicKeyTest ()
    {
        beginTest ("compare");

        ValidatorInfo::PublicKey one (ValidatorInfo::PublicKey::createFromInteger (1U));
        ValidatorInfo::PublicKey two (ValidatorInfo::PublicKey::createFromInteger (2U));

        expect (one < two, "should be less");
        expect (two > one, "should be greater");
    }

    void fetchTest ()
    {
        beginTest ("fetch");

        TestValidatorSource source (0, 32);

        ValidatorsImp::ValidatorInfoArray results;

        source.fetch (results);

        expect (results.size () == 32, "size should be 32");
    }

    void runTest ()
    {
        publicKeyTest ();

        fetchTest ();
    }
};

template class UnitTestType <ValidatorListTests>;

/*

Goal:

  Provide the listener with a ValidatorList.
  - This forms the UNL

Task:

  fetch ValidatorInfo array from a source

  - We have the old one and the new one, compute the following:

    * unchanged validators list
    * new validators list
    * removed validators list

  - From the unchanged / new / removed, figure out what to do.

*/


