#include "tagger.hpp"
using MeCab::createTagger;
using MeCab::createModel;
#include <iostream>
#include <future>
#include <thread>
#include <utility>


NodeWrapper::NodeWrapper(const std::string & surface, const std::string &feature, 
            size_t id, size_t length, size_t rlength, long wcost, long cost):
        surface(surface), feature(feature), id(id),length(length), rlength(rlength), 
        wcost(wcost), cost(cost)
{}

        NodeWrapper::NodeWrapper(const mecab_node_t * q):
            surface(std::string{q->surface}.substr(0, q->length)),
            feature(q->feature),
            id(q->id),
            length(q->length),
            rlength(q->rlength),
            wcost(q->wcost),
            cost(q->cost)
{}

std::list<NodeWrapper> expand_node(const mecab_node_t * q) {
    std::list<NodeWrapper> result;
    while (q) {
        result.emplace_back(q);
        q = q->next;
    };
    return result;
}

TaggerWrapper::TaggerWrapper(const std::string & arg):
    model_(createModel(arg.c_str())),
    tagger_(model_->createTagger())
{
    CHECK(tagger_.get());
}

TaggerWrapper::TaggerWrapper(): TaggerWrapper("") {
    CHECK(tagger_.get());
}

std::list<NodeWrapper> TaggerWrapper::parse_to_node(const std::string & arg) {
    auto q = tagger_->parseToNode(arg.c_str(), arg.size()); 
    return expand_node(q);
}

std::list<NodeWrapper> TaggerWrapper::parse_to_node_with_lattice(
    const std::string & arg,  MeCab::Lattice* lattice) {
    lattice->set_sentence(arg.c_str());
    bool success = tagger_->parse(lattice); 
    if (!success) {
        throw std::runtime_error("Somehow the parse failed");
    }
    auto bos = lattice->bos_node();

    return expand_node(bos);
}

std::vector<std::list<NodeWrapper>> TaggerWrapper::parse_to_node_parallel(
        const std::vector<std::string> & args, size_t n_workers) {
    if (args.empty()) return {};
    if (n_workers ==0)
        throw std::invalid_argument("n_workers must be a strictly positive number.");

    n_workers = n_workers < args.size() ? n_workers : args.size();
    using nodes_with_index = std::vector<std::pair<size_t, std::list<NodeWrapper>>>;

    std::vector<std::future<nodes_with_index>> async_results;
    nodes_with_index result_accumulator;
    std::vector<std::list<NodeWrapper>> result(args.size());

    for (size_t i = 0; i < n_workers; i++) {
        auto futres = std::async(std::launch::async, [this, &args, n_workers, i](){
            MeCab::Lattice * lattice = this->model_->createLattice();
            size_t j = i;
            nodes_with_index result;
            while (j < args.size()) {
                std::list<NodeWrapper> parse_result = this->parse_to_node_with_lattice(
                    args[j], std::move(lattice)
                );
                result.push_back({j, std::move(parse_result)});
                j += n_workers;
            }
            return result;
        });
        async_results.push_back(std::move(futres));
    }
    for (size_t i = 0; i < n_workers; i++) {
        auto local_result = async_results[i].get();
        for(size_t j = 0; j <  local_result.size(); j++) {
            result_accumulator.emplace_back(std::move(local_result[j]));
        }
    }
    for (size_t j = 0; j < args.size(); j++) {
        size_t index = result_accumulator[j].first;
        result[index] = std::move(result_accumulator[j].second);
    }
    return result;
}


std::list<std::list<NodeWrapper>>
TaggerWrapper::parse_n_best(const std::string & arg, size_t n) {
    std::unique_ptr<MeCab::Lattice> lattice(MeCab::createLattice());
    lattice->set_request_type(MECAB_NBEST);
    lattice->set_sentence(arg.c_str());
    bool success = tagger_->parse(lattice.get());
    if (!success) {
        throw std::runtime_error("Somehow the parsing failed..");
    }
    auto bos = lattice->bos_node();
    std::list<std::list<NodeWrapper>> result;
    for (size_t i = 0; i < n; i++) {
        result.emplace_back(expand_node(bos));
        bool has_next = lattice->next();
        if (!has_next) break;
        bos = lattice->bos_node();
    }
    return result;
}


namespace py = pybind11;
PYBIND11_MODULE(mecab_pybind, m) {
    m.doc() = "mylibs made by pybind11";

    py::class_<NodeWrapper>(m, "Node")
        .def_readonly("surface", &NodeWrapper::surface)
        .def_readonly("feature", &NodeWrapper::feature)
        .def_readonly("id", &NodeWrapper::id) 
        .def_readonly("length", &NodeWrapper::length)
        .def_readonly("rlength", &NodeWrapper::rlength) 
        .def_readonly("cost", &NodeWrapper::cost)
        .def_readonly("wcost", &NodeWrapper::wcost)
        .def("__repr__", [](const NodeWrapper & arg) {
            std::stringstream ss;
            ss << "<Node with surface=\"" << arg.surface 
                << "\", feature = \"" << arg.feature << "\">";
            return ss.str();
        });

    py::class_<TaggerWrapper>(m, "Tagger")
        .def(py::init<const std::string &>())
        .def(py::init<>())
        .def("parse_to_node", &TaggerWrapper::parse_to_node)
        .def("parse_to_node_parallel", &TaggerWrapper::parse_to_node_parallel) 
        .def("parse_n_best", &TaggerWrapper::parse_n_best);

};

