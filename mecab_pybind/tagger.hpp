#ifndef MACAB_PYBIND_TAGGER
#define MACAB_PYBIND_TAGGER
#include <iostream>
#include <mecab.h>
#include <memory>
#include <list>
#include <type_traits>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // vector用
#include <sstream>
#include <thread>
#include <tuple>
#include <mutex>

#define CHECK(eval) if (! eval) { \
   const char *e = eval ? eval->what() : MeCab::getTaggerError(); \
   std::cerr << "Exception:" << e << std::endl; \
   throw std::runtime_error("Mecab Exception."); }

// Since MeCab has its own memory management cycle, in order not to doubly free
// the allocated memory, we copy the struct.

struct NodeWrapper {
    NodeWrapper(const std::string & surface, const std::string &feature, 
            size_t id, size_t length, size_t rlength, long wcost, long cost);
    NodeWrapper(const mecab_node_t * q);
    const std::string surface;
    const std::string feature;

    const size_t id, length, rlength;
    const long wcost, cost;
};

struct TaggerWrapper {
    TaggerWrapper();
    TaggerWrapper(const std::string & arg);
    std::list<NodeWrapper> parse_to_node(const std::string & arg);
    std::list<NodeWrapper> parse_to_node_with_lattice(const std::string & arg, MeCab::Lattice* lattice);
    std::vector<std::list<NodeWrapper>> parse_to_node_parallel(
        const std::vector<std::string> & arg, size_t n_workers);

    std::list<std::list<NodeWrapper>>
        parse_n_best(const std::string & arg, size_t n);
    private:
    std::unique_ptr<MeCab::Model> model_;
    std::unique_ptr<MeCab::Tagger> tagger_;

};

#endif

