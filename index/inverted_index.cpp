/**
 * @file inverted_index.cpp
 */

#include "inverted_index.h"

InvertedIndex::InvertedIndex(const string & lexiconFile, const string & postingsFile, Tokenizer* tokenizer):
    _lexicon(lexiconFile),
    _postings(postingsFile),
    _tokenizer(tokenizer)
{ /* nothing */ }

multimap<double, string> InvertedIndex::search(const Document & query) const
{
    cerr << "[InvertedIndex]: scoring documents for query " << query.getName()
         << " (" << query.getCategory() << ")" << endl;

    double k1 = 1.5;
    double b = 0.75;
    double k3 = 500;
    double numDocs = _lexicon.getNumDocs();
    double avgDL = _lexicon.getAvgDocLength();
    unordered_map<DocID, double> scores;

    const unordered_map<TermID, unsigned int> freqs = query.getFrequencies();
    for(auto & queryTerm: freqs)
    {
        TermID queryTermID = queryTerm.first;
        size_t queryTermFreq = queryTerm.second;
        TermData termData = _lexicon.getTermInfo(queryTermID);
        vector<PostingData> docList = _postings.getDocs(termData);
        for(auto & doc: docList)
        {
            double docLength = _lexicon.getDocLength(doc.docID);
            double IDF = log((numDocs - termData.idf + 0.5) / (termData.idf + 0.5));
            double TF = ((k1 + 1.0) * doc.freq) / ((k1 * ((1.0 - b) + b * docLength / avgDL)) + doc.freq);
            double QTF = ((k3 + 1.0) * queryTermFreq) / (k3 + queryTermFreq);
            double score = TF * IDF * QTF;
            cerr << queryTermID << ": IDF: " << IDF << ", TF: " << TF << ", QTF: " << QTF << endl;
            scores[doc.docID] += score;
        }
    }

    // combine into sorted multimap
    multimap<double, string> results;
    for(auto & score: scores)
    {
        cerr << score.second << ": " << _lexicon.getDoc(score.first) << endl;
        results.insert(make_pair(score.second, Document::getCategory(_lexicon.getDoc(score.first))));
    }
    return results;
}

string InvertedIndex::classifyKNN(const Document & query, size_t k) const
{
    multimap<double, string> ranking = search(query);
    unordered_map<string, size_t> counts;
    size_t numResults = 0;
    for(auto result = ranking.rbegin(); result != ranking.rend() && numResults++ != k; ++result)
    {
        size_t space = result->second.find_first_of(" ") + 1;
        string category = result->second.substr(space, result->second.size() - space);
        counts[category]++;
    }

    string best = "[no results]";
    size_t high = 0;
    for(auto & count: counts)
    {
        if(count.second > high)
        {
            best = count.first;
            high = count.second;
        }
    }

    return best;
}

bool InvertedIndex::indexDocs(vector<Document> & documents, size_t chunkMBSize)
{
    if(!_lexicon.isEmpty())
    {
        cerr << "[InvertedIndex]: attempted to create an index in an existing index location" << endl;
        return false;
    }

    size_t numChunks = _postings.createChunks(documents, chunkMBSize, _tokenizer);
    _tokenizer->saveTermIDMapping("termid.mapping");
    _postings.saveDocIDMapping("docid.mapping");
    _postings.createPostingsFile(numChunks, _lexicon);
    _postings.saveDocLengths(documents, "docs.lengths");
    _lexicon.save("docs.lengths", "termid.mapping", "docid.mapping");

    return true;
}