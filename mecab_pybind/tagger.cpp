#include "tagger.hpp"
using MeCab::createTagger;
using MeCab::createModel;

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
    base_tagger(createTagger(arg.c_str()))
{
    CHECK(base_tagger.get());
}

TaggerWrapper::TaggerWrapper(): base_tagger(createTagger("")) {
    CHECK(base_tagger.get());
}

std::list<NodeWrapper> TaggerWrapper::parse_to_node(const std::string & arg) {
    auto q = base_tagger->parseToNode(arg.c_str(), arg.size()); 
    return expand_node(q);
}

std::list<std::list<NodeWrapper>>
TaggerWrapper::parse_n_best(const std::string & arg, size_t n) {
    std::unique_ptr<MeCab::Lattice> lattice(MeCab::createLattice());
    lattice->set_request_type(MECAB_NBEST);
    lattice->set_sentence(arg.c_str());
    bool q = base_tagger->parse(lattice.get());
    if (!q) {
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
        .def("parse_n_best", &TaggerWrapper::parse_n_best);

};

