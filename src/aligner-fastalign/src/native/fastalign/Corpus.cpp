//
// Created by Davide  Caroselli on 23/08/16.
//


#include <iostream>

#include "Corpus.h"
#include "Vocabulary.h"
#include <boost/filesystem.hpp>


using namespace std;
using namespace mmt;
using namespace mmt::fastalign;

namespace fs = boost::filesystem;

Corpus::Corpus(const std::string &sourceFile, const std::string &targetFile) :
        name(fs::path(sourceFile).stem().string()), sourceFile(sourceFile), targetFile(targetFile) {
}

void Corpus::List(const std::string &path, const std::string &sourceLang, const std::string &targetLang,
                  std::vector<Corpus> &outList) {
    fs::recursive_directory_iterator endit;

    for (fs::recursive_directory_iterator it(path); it != endit; ++it) {
        fs::path file = fs::absolute(*it);

        if (!fs::is_regular_file(file))
            continue;

        if (file.extension().string() == "." + sourceLang) {
            fs::path sourceFile = file;
            fs::path targetFile = file;

            targetFile = targetFile.replace_extension(fs::path(targetLang));

            if (!fs::is_regular_file(targetFile))
                continue;

            outList.push_back(Corpus(sourceFile.string(), targetFile.string()));
        }
    }
}

static inline void ParseLine(const string &line, sentence_t &output) {
    output.clear();

    std::stringstream stream(line);
    std::string word;

    while (std::getline(stream, word, ' '))
        output.push_back(word);
}

static inline void ParseLine(const Vocabulary *vocab, const string &line, wordvec_t &output) {
    output.clear();

    std::stringstream stream(line);
    std::string word;

    while (std::getline(stream, word, ' '))
        output.push_back(vocab->Get(word));
}

CorpusReader::CorpusReader(const Corpus &corpus, const Vocabulary *vocabulary,
                           const size_t maxLineLength, const bool skipEmptyLines)
        : drained(false), vocabulary(vocabulary), source(corpus.sourceFile.c_str()), target(corpus.targetFile.c_str()),
          maxLineLength(maxLineLength), skipEmptyLines(skipEmptyLines) {
}

bool CorpusReader::Read(sentence_t &outSource, sentence_t &outTarget) {
    if (drained)
        return false;

    string sourceLine, targetLine;
    while (true) {
        if (!getline(source, sourceLine) || !getline(target, targetLine)) {
            drained = true;
            return false;
        }

        ParseLine(sourceLine, outSource);
        ParseLine(targetLine, outTarget);

        if (Skip(outSource, outTarget))
            continue;

        return true;
    }
}

bool CorpusReader::Read(vector<pair<sentence_t, sentence_t>> &outBuffer, size_t limit) {
    if (drained)
        return false;

    vector<pair<string, string>> batch;
    for (size_t i = 0; i < limit; ++i) {
        string sourceLine, targetLine;
        if (!getline(source, sourceLine) || !getline(target, targetLine)) {
            drained = true;
            break;
        }

        batch.push_back(pair<string, string>(sourceLine, targetLine));
    }

    if (batch.empty())
        return false;

    outBuffer.resize(batch.size());
#pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < batch.size(); ++i) {
        ParseLine(batch[i].first, outBuffer[i].first);
        ParseLine(batch[i].second, outBuffer[i].second);
    }

    if (skipEmptyLines || maxLineLength > 0) {
        for (auto sentence = outBuffer.begin(); sentence != outBuffer.end(); /* no increment */) {
            if (Skip(sentence->first, sentence->second))
                sentence = outBuffer.erase(sentence);
            else
                ++sentence;
        }
    }

    return !outBuffer.empty();
}

bool CorpusReader::Read(wordvec_t &outSource, wordvec_t &outTarget) {
    if (drained)
        return false;

    string sourceLine, targetLine;
    while (true) {
        if (!getline(source, sourceLine) || !getline(target, targetLine)) {
            drained = true;
            return false;
        }

        ParseLine(vocabulary, sourceLine, outSource);
        ParseLine(vocabulary, targetLine, outTarget);

        if (Skip(outSource, outTarget))
            continue;

        return true;
    }
}

bool CorpusReader::Read(std::vector<std::pair<wordvec_t, wordvec_t>> &outBuffer, size_t limit) {
    if (drained)
        return false;

    vector<pair<string, string>> batch;
    for (size_t i = 0; i < limit; ++i) {
        string sourceLine, targetLine;
        if (!getline(source, sourceLine) || !getline(target, targetLine)) {
            drained = true;
            break;
        }

        batch.push_back(pair<string, string>(sourceLine, targetLine));
    }

    if (batch.empty())
        return false;

    outBuffer.resize(batch.size());
#pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < batch.size(); ++i) {
        ParseLine(vocabulary, batch[i].first, outBuffer[i].first);
        ParseLine(vocabulary, batch[i].second, outBuffer[i].second);
    }

    if (skipEmptyLines || maxLineLength > 0) {
        for (auto sentence = outBuffer.begin(); sentence != outBuffer.end(); /* no increment */) {
            if (Skip(sentence->first, sentence->second))
                sentence = outBuffer.erase(sentence);
            else
                ++sentence;
        }
    }

    return !outBuffer.empty();
}
