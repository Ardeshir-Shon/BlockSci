//
//  blockchain_py.cpp
//  blocksci
//
//  Created by Harry Kalodner on 7/4/17.
//
//

#include "variant_py.hpp"

#include <blocksci/chain/algorithms.hpp>
#include <blocksci/chain/blockchain.hpp>
#include <blocksci/chain/transaction.hpp>
#include <blocksci/index/address_index.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <range/v3/view/any_view.hpp>
#include <range/v3/view/slice.hpp>
#include <range/v3/view/stride.hpp>

namespace py = pybind11;

using namespace blocksci;

void init_blockchain(py::module &m) {
    py::class_<Blockchain> cl(m, "Blockchain", "Class representing the blockchain. This class is contructed by passing it a string representing a file path to your BlockSci data files generated by blocksci_parser", py::dynamic_attr());
    cl
    .def(py::init<std::string>())
    .def("__len__", [](const Blockchain &chain) {
        return chain.size();
    }, "Returns the total number of blocks in the blockchain")
    /// Optional sequence protocol operations
    .def("__iter__", [](const Blockchain &chain) { return py::make_iterator(chain.begin(), chain.end()); },
         py::keep_alive<0, 1>(), "Allows direct iteration over the blocks in the blockchain")
    .def("__getitem__", [](const Blockchain &chain, int64_t i) {
        if (i < 0) {
            i = (chain.size() + i) % chain.size();
        }
        uint64_t posIndex = static_cast<uint64_t>(i);
        if (posIndex >= chain.size()) {
            throw py::index_error();
        }
        return chain[i];
    }, "Return the block of the given height")
    .def("__getitem__", [](const Blockchain &chain, py::slice slice) -> std::vector<Block> {
        size_t start, stop, step, slicelength;
        if (!slice.compute(chain.size(), &start, &stop, &step, &slicelength))
            throw py::error_already_set();
        return chain | ranges::view::slice(start, stop) | ranges::view::stride(step) | ranges::to_vector;
    }, "Return a list of blocks with their heights in the given range")
    .def("segment", segmentChain, "Divide the blockchain into the given number of chunks with roughly the same number of transactions in each")
    .def("coinjoin_txes", getCoinjoinTransactions, "Returns a list of all transactions that might be JoinMarket coinjoin transactions")
    .def("possible_coinjoin_txes", getPossibleCoinjoinTransactions, "Returns a list of all transactions that might be coinjoin transactions")
    .def("script_type_txes", getTransactionIncludingOutput, "Returns a list of all transactions that include outputs of the given script type")
    .def("script_deanon_txes", getDeanonTxes, "Return a list of transaction for which is_script_deanon returns true")
    .def("change_script_type_txes", getChangeOverTxes, "Return a list of transaction for which is_change_over returns true")
    .def("keyset_change_txes", getKeysetChangeTxes, "Return a list of transaction for which is_keyset_change returns true")
    .def("scripts", [](const Blockchain &chain, ScriptType::Enum type) {
        return chain.scripts(type);
    })
    ;
}
