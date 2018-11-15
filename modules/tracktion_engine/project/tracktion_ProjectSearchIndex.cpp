/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct IndexedWord
{
    String word;
    int numIDs;
    HeapBlock<int> ids;

    IndexedWord (InputStream& in)  : word (in.readString())
    {
        numIDs = in.readShort();
        ids.malloc ((size_t) numIDs);
        in.read (ids, (int) sizeof (int) * numIDs);
    }

    IndexedWord (const String& w, int mid) : word (w), numIDs (1)
    {
        ids.malloc (1);
        *ids = mid;
    }

    void writeToStream (OutputStream& out)
    {
        out.writeString (word);
        out.writeShort ((short) numIDs);
        out.write (ids, (size_t) numIDs * sizeof (int));
    }

    void addMediaId (int mid)
    {
        for (int i = numIDs; --i >= 0;)
            if (ids[i] == mid)
                return;

        if (numIDs > 32766)
        {
            jassertfalse;
            return;
        }

        ids.realloc ((size_t) ++numIDs);
        ids[numIDs - 1] = mid;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IndexedWord)
};

//==============================================================================
ProjectSearchIndex::ProjectSearchIndex (Project& p) : project (p)
{
}

static bool isNoiseWord (const String& word)
{
    return     word == "a"
            || word == "the"
            || word == "of"
            || word == "to"
            || word == "for"
            || word == "from"
            || word == "and"
            || word == "in"
            || word == "on"
            || word == "at"
            || word == "by"
            || word == "or"
            || word == "some"
            || word == "into"
            || word == "onto"
            || word == "as"
            || word == "it"
            || word == "is"
            || word == "few"
            || word == "are"
            || word == "if"
            || word == "like"
            || word == "then"
            || word == "that"
            || word == "this"
            || word == "not"
            || word == "but";
}

struct IndexSorter
{
    static int compareElements (IndexedWord* first, IndexedWord* second)
    {
        return first->word.compare (second->word);
    }
};

void ProjectSearchIndex::addClip (const ProjectItem::Ptr& item)
{
    if (item != nullptr)
    {
        for (auto newWord : item->getSearchTokens())
        {
            auto word = newWord.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789");

            if (! (word.isEmpty() || isNoiseWord (word)))
            {
                if (auto w = findWordMatch (word))
                {
                    w->addMediaId (item->getID().getItemID());
                }
                else
                {
                    auto nw = new IndexedWord (word, item->getID().getItemID());

                    IndexSorter sorter;
                    index.addSorted (sorter, nw);
                }
            }
        }
    }
}

void ProjectSearchIndex::writeToStream (OutputStream& out)
{
    out.writeInt (index.size());

    for (auto&& i : index)
        i->writeToStream (out);
}

void ProjectSearchIndex::readFromStream (InputStream& in)
{
    index.clear();

    for (int i = in.readInt(); --i >= 0;)
        index.add (new IndexedWord (in));
}

IndexedWord* ProjectSearchIndex::findWordMatch (const String& word) const
{
    int start = 0;
    int end = index.size();

    for (;;)
    {
        if (start >= end)
            break;

        auto w = index.getUnchecked (start);

        if (word == w->word)
            return w;

        const int halfway = (start + end) >> 1;

        if (halfway == start)
            break;

        if (word.compare (index[halfway]->word) >= 0)
            start = halfway;
        else
            end = halfway;
    }

    return {};
}

void ProjectSearchIndex::findMatches (SearchOperation& search, Array<ProjectItemID>& results)
{
    for (auto& res : search.getMatches (*this))
        results.add (ProjectItemID (res, project.getProjectID()));
}


//==============================================================================
SearchOperation::SearchOperation (SearchOperation* o1, SearchOperation* o2) : in1 (o1), in2 (o2)
{
}

SearchOperation::~SearchOperation()
{
}

//==============================================================================
struct WordMatchOperation : public SearchOperation
{
    WordMatchOperation (const String& w) : word (w.toLowerCase().trim()) {}

    Array<int> getMatches (ProjectSearchIndex& psi) override
    {
        Array<int> found;

        if (auto w = psi.findWordMatch (word))
            for (int j = 0; j < w->numIDs; ++j)
                found.add (w->ids[j]);

        return found;
    }

    String word;
};

struct OrOperation : public SearchOperation
{
    OrOperation (SearchOperation* in1, SearchOperation* in2)  : SearchOperation (in1, in2) {}

    Array<int> getMatches (ProjectSearchIndex& psi) override
    {
        auto i1 = in1->getMatches (psi);
        auto i2 = in2->getMatches (psi);

        if (i1.isEmpty())
            return i2;

        if (i2.isEmpty())
            return i1;

        for (int i = i2.size(); --i >= 0;)
            i1.addIfNotAlreadyThere (i2[i]);

        return i1;
    }
};

struct AndOperation : public SearchOperation
{
    AndOperation (SearchOperation* in1, SearchOperation* in2) : SearchOperation (in1, in2) {}

    Array<int> getMatches (ProjectSearchIndex& psi) override
    {
        auto i1 = in1->getMatches (psi);

        if (i1.isEmpty())
            return i1;

        auto i2 = in2->getMatches (psi);

        if (i2.isEmpty())
            return i2;

        i1.removeValuesNotIn (i2);
        return i1;
    }
};

struct NotOperation : public SearchOperation
{
    NotOperation (SearchOperation* in1) : SearchOperation (in1, nullptr) {}

    Array<int> getMatches (ProjectSearchIndex& psi)
    {
        Array<int> i1;

        i1 = psi.project.getAllItemIDs();
        i1.removeValuesIn (in1->getMatches (psi));

        return i1;
    }
};

struct FalseOperation  : public SearchOperation
{
    Array<int> getMatches (ProjectSearchIndex&) override
    {
        return {};
    }
};

//==============================================================================
SearchOperation* createPluralOptions (String s)
{
    SearchOperation* c = new WordMatchOperation (s);

    if (s.length() > 2 && ! (s.endsWith ("a") || s.endsWith ("i") || s.endsWith ("u")))
    {
        if (s.endsWith ("e") && ! (s.endsWith ("ee") || s.endsWith ("ie")))
        {
            c = new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (1) + "ing"));
        }
        else
        {
            if (s.endsWith ("ss") || ! s.endsWith ("s"))
            {
                c = new OrOperation (c, new WordMatchOperation (s + "ing"));
            }
            else
            {
                if (s.endsWith ("es"))
                    c = new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (2) + "ing"));
                else
                    c = new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (1) + "ing"));
            }
        }
    }

    if (s.endsWith ("ing") && s.length() > 4)
    {
        s = s.dropLastCharacters (3);
        c = new OrOperation (c, new WordMatchOperation (s));
        c = new OrOperation (c, new WordMatchOperation (s + "e"));
    }

    if (s.endsWith ("s"))
    {
        if (s.endsWith ("shes") || s.endsWith ("ches"))
            c = new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (2)));
        else if (s.endsWith ("ses"))
            c = new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (2)));
        else if (s.endsWith ("ss"))
            return c;
        else if (s.endsWith ("ies"))
            c = new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (3) + "y"));
        else if (s.endsWith ("ves"))
            c = new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (3) + "f"));
        else
            c = new OrOperation (new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (1))),
                                 new WordMatchOperation (s + "es"));
    }
    else if (s.endsWith ("f"))
    {
        c = new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (1) + "ves"));
    }
    else if (s.endsWith ("y"))
    {
        c = new OrOperation (c, new WordMatchOperation (s.dropLastCharacters (1) + "ies"));
    }
    else if (s.endsWith ("sh") || s.endsWith ("ch"))
    {
        c = new OrOperation (c, new WordMatchOperation (s + "es"));
    }
    else
    {
        c = new OrOperation (c, new WordMatchOperation (s + "s"));
    }

    return c;
}

SearchOperation* multipleWordMatch (const String& s)
{
    StringArray a;
    a.addTokens (s, false);

    SearchOperation* c = nullptr;

    for (int i = a.size(); --i >= 0;)
    {
        if (c == nullptr)
            c = new WordMatchOperation (a[i]);
        else
            c = new AndOperation (c, new WordMatchOperation (a[i]));
    }

    return c;
}

SearchOperation* createCondition (const StringArray& words, int start, int length)
{
    if (length == 0)
        return {};

    if (length == 1)
    {
        if (words[start] == TRANS("All"))
            return new NotOperation (new FalseOperation());

        return createPluralOptions (words[start]);
    }

    if (length > 1 && words[start] == TRANS("Not"))
        return new NotOperation (createCondition (words, start + 1, length - 1));

    if (length > 2)
    {
        auto c = createCondition (words, start, 1);

        if (words[start + 1] == TRANS("Or"))
            return new OrOperation (c, createCondition (words, start + 2, length - 2));

        if (words[start + 1] == TRANS("And"))
            return new AndOperation (c, createCondition (words, start + 2, length - 2));

        if (words[start + 1] == TRANS("But")
             && words[start + 2] == TRANS("Not")
             && length > 3)
            return new AndOperation (c, new NotOperation (createCondition (words, start + 3, length - 3)));

        if (words[start + 1] == TRANS("Not"))
            return new AndOperation (c, new NotOperation (createCondition (words, start + 2, length - 2)));
    }

    return new AndOperation (createCondition (words, start, 1),
                             createCondition (words, start + 1, length - 1));
}


SearchOperation* createSearchForKeywords (const String& keywords)
{
    const String k (keywords.toLowerCase()
                            .replace ("-", " " + TRANS("Not") + " ")
                            .replace ("+", " " + TRANS("And") + " ")
                            .retainCharacters (CharPointer_UTF8 ("abcdefghijklmnopqrstuvwxyz0123456789\xc3\xa0\xc3\xa1\xc3\xa2\xc3\xa3\xc3\xa4\xc3\xa5\xc3\xa6\xc3\xa7\xc3\xa8\xc3\xa9\xc3\xaa\xc3\xab\xc3\xac\xc3\xad\xc3\xae\xc3\xaf\xc3\xb0\xc3\xb1\xc3\xb2\xc3\xb3\xc3\xb4\xc3\xb5\xc3\xb6\xc3\xb8\xc3\xb9\xc3\xba\xc3\xbb\xc3\xbc\xc3\xbd\xc3\xbf\xc3\x9f"))
                            .trim());

    StringArray words;
    words.addTokens (k, false);
    words.trim();
    words.removeEmptyStrings();

    for (int i = words.size(); --i >= 0;)
        if (isNoiseWord (words[i])
             && words[i] != TRANS("Not")
             && words[i] != TRANS("Or")
             && words[i] != TRANS("And"))
            words.remove (i);

    if (auto sc = createCondition (words, 0, words.size()))
        return sc;

    return new FalseOperation();
}
