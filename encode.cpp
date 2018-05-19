#include <iostream>
#include <algorithm>
#include <fstream>
#include <unordered_map>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/filesystem.hpp>

#include "codecs.hpp"
#include "util.hpp"
#include "hash_utils.hpp"
#include "binary_collection.hpp"
#include "dictionary.hpp"

using namespace ds2i;

template<typename Encoder>
void encode(char const* collection_name,
            char const* output_filename,
            char const* dictionary_filename)
{
    binary_collection input(collection_name);

    uint64_t num_processed_lists = 0;
    uint64_t num_total_ints = 0;

    bool take_gaps = true;
    boost::filesystem::path collection_path(collection_name);
    if (collection_path.extension() == ".freqs") {
        take_gaps = false;
        logger() << "not taking d-gaps" << std::endl;
    } else if (collection_path.extension() == ".docs") {
        logger() << "taking d-gaps" << std::endl;
    } else {
        throw std::runtime_error("unsupported file format");
    }

    dictionary::builder builder;

    if (dictionary_filename) {
        std::ifstream dictionary_file(dictionary_filename);
        builder.load(dictionary_file);
        builder.prepare_for_encoding();
    }

    std::vector<uint8_t> output;
    const static uint64_t GB = 1073741824;
    uint64_t bytes = 5 * GB;
    output.reserve(bytes);

    std::vector<uint32_t> buf;

    logger() << "encoding..." << std::endl;

    for (auto const& list: input)
    {
        uint32_t n = list.size();
        if (n > MIN_SIZE)
        {
            buf.reserve(n);
            uint32_t prev = 0;
            uint32_t universe = 0;

            for (auto b = list.begin(); b != list.end(); ++b) {
                buf.push_back(*b - prev);
                if (take_gaps) {
                    prev = *b;
                }
                universe += buf.back();
            }
            assert(buf.size() == n);

            header::write(n, universe, output);
            Encoder::encode(buf.data(), universe, n, output, &builder);
            buf.clear();

            ++num_processed_lists;
            num_total_ints += n;
            // break; // encode just the first sequence

            if (num_processed_lists % 1000 == 0) {
                logger() << "encoded " << num_processed_lists << " lists" << std::endl;
                logger() << "encoded " << num_total_ints << " integers" << std::endl;
                logger() << "bits x integer: "
                         << output.size() * sizeof(output[0]) * 8.0 / num_total_ints << std::endl;
            }
        }
    }

    logger() << "encoded " << num_processed_lists << " lists" << std::endl;
    logger() << "encoded " << num_total_ints << " integers" << std::endl;
    logger() << "bits x integer: "
             << output.size() * sizeof(output[0]) * 8.0 / num_total_ints << std::endl;

    if (output_filename) {
        logger() << "writing encoded data..." << std::endl;
        std::ofstream output_file(output_filename);
        output_file.write(reinterpret_cast<char const*>(output.data()),
                          output.size() * sizeof(output[0]));
        output_file.close();
        logger() << "DONE" << std::endl;
    }
}

int main(int argc, char** argv) {

    if (argc < 3) {
        std::cerr << "Usage " << argv[0] << ":\n"
                  << "\t<type> <collection_name> [--dict <dictionary_filename>] [--out <output_filename>]"
                  << std::endl;
        return 1;
    }

    std::string type = argv[1];
    char const* collection_name = argv[2];
    char const* dictionary_filename = nullptr;
    char const* output_filename = nullptr;

    std::string cmd(type + " " + std::string(collection_name));

    for (int i = 3; i < argc; ++i) {
        if (argv[i] == std::string("--dict")) {
            ++i;
            dictionary_filename = argv[i];
            cmd += " --dict " + std::string(dictionary_filename);
        } else if (argv[i] == std::string("--out")) {
            ++i;
            output_filename = argv[i];
            cmd += " --out " + std::string(output_filename);
        } else {
            throw std::runtime_error("unknown parameter");
        }
    }

    logger() << cmd << std::endl;

    if (false) {
#define LOOP_BODY(R, DATA, T)                                               \
        } else if (type == BOOST_PP_STRINGIZE(T)) {                         \
            encode<BOOST_PP_CAT(T, )>                                         \
                (collection_name, output_filename, dictionary_filename);    \
            /**/

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, CODECS);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: unknown type '"
                 << type << "'" << std::endl;
    }

    return 0;
}