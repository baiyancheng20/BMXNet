/*!
 * Copyright (c) 2015 by Contributors
 * \file symbolic.h
 * \brief Symbolic interface of mxnet.
 * \author Min Lin, Bing Xu
*/
#ifndef MXNET_SYMBOLIC_H_
#define MXNET_SYMBOLIC_H_

#include <dmlc/base.h>
#include <vector>
#include <memory>
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include "./base.h"
#include "./narray.h"
#include "./operator.h"

// check c++11
#if DMLC_USE_CXX11 == 0
#error "CXX11 was required for symbolic module"
#endif

namespace mxnet {
/*!
 * \brief StaticGraph is the configuration of computation graphs.
 *  This is the "configuration file" of mxnet.
 *  It can be converted to/from Symbol, and can be used to bind to operators.
 */
class StaticGraph {
 public:
  /*! \brief represents a data in the graph */
  struct DataEntry {
    /*! \brief the source node id in the computation graph */
    uint32_t source_id;
    /*! \brief index of output from the source. */
    uint32_t index;
  };
  /*! \brief Operation Node in static graph */
  struct Node {
    /*! \brief wrapped operator property */
    std::unique_ptr<OperatorProperty> op;
    /*! \brief name of the node */
    std::string name;
    /*! \brief inputs (node_id, index) for of the nodes*/
    std::vector<DataEntry> inputs;
  };
  /*! \brief all nodes in the graph */
  std::vector<Node> nodes;
  /*! \brief index is nodes that correspods to arguments */
  std::vector<uint32_t> arg_nodes;
  /*! \brief outputs(heads) of the graph */
  std::vector<DataEntry> outputs;
  // funtions to help inference in static graph
  /*!
   * \brief Perform a topological sort on the graph
   * \return a topological order of node indices.
   */
  std::vector<uint32_t> TopoSort() const;
  /*!
   * \brief infer the node shapes in the computation graph.
   *
   *  When calling this function, user can setup the shape information known into right position.
   *  Unknown shape are indicated by shape.ndim() == 0.
   *
   * \param topo_order The topological order of node index, as created by TopoSort.
   * \param node_out_shapes The shapes of the each outputs of nodes in the graph.
   * \return if the shape inference is successful, return true, else return false.
   */
  bool InferNodeShapes(const std::vector<uint32_t> &topo_order,
                       std::vector<std::vector<TShape> > *node_out_shapes) const;
  /*!
   * \brief infer the shapes of outputs and unknown input arguments
   * \param in_shape the shape of input arguments of the operator
   *     this should be of same length as the vector returned by ListArguments
   *     in_shape allows unknown elements, which are checked by shape.ndim() == 0.
   *     For unknown shapes, InferShape will try to fill in the correct Shape in in_shape
   *     For known shapes, InferShape will check shape consistency
   *
   *     common practice: set the shape of data input, and usually weight's shape can be infered
   *
   * \param out_shape the shape of outputs of the operator
   *     InferShape will modify the vector to fill output TShape
   * \return if the shape inference is successful, return true, else return false.
   */
  bool InferShape(std::vector<TShape> *in_shape,
                  std::vector<TShape> *out_shape) const;
};

/*!
 * \brief Symbol is used to represent dynamically generated symbolic computation graph.
 *
 *   This class is used as a tool to generate computation graphs(aka. configuration) of the network.
 *   Symbol is always composite, the head Node is the output node of the symbol.
 *   An atomic symbol can be seen as a special case of the composite symbol with only the head node.
 *
 *   The symbol can be converted from/to StaticGraph, the actual configuration used by mxnet.
 *   Symbol offers more flexible way to composite nodes than StaticGraph, which makes it good
 *   tool to generate configurations from language bindings such as python.
 * \sa StaticGraph
 */
class Symbol {
 public:
  /*!
   * \brief copy the symbol
   * \return a deep copy of the graph
   */
  Symbol Copy() const;
  /*!
   * \brief print the symbol info to output stream.
   * \param os the output stream we like to print to
   */
  void Print(std::ostream &os) const; // NOLINT(*)
  /*!
   * \brief List the arguments names.
   *
   * The position of the returned list also corresponds to calling position in operator()
   * \return the arguments list of this symbol, they can be either named or unnamed (empty string).
   */
  std::vector<std::string> ListArguments() const;
  /*! \return get the descriptions of outputs for this symbol */
  std::vector<std::string> ListReturns() const;
  /*!
   * \brief get the index th element from the returned tuple.
   * \param index index of multi output
   * \return the symbol corresponds to the indexed element.
   */
  Symbol operator[] (size_t index) const;
  /*!
   * \brief Compose the symbol with arguments, this changes current symbol.
   *
   * The positional arguments passed in must be complete(contain all arguments).
   *
   * \param args positional arguments for the symbol
   * \param name name of returned symbol.
   */
  void Compose(const std::vector<Symbol>& args,
               const std::string& name);
  /*!
   * \brief Compose the symbol with arguments, this changes the current symbol.
   * The kwargs passed in can be in-complete,
   *
   * The rest of the symbols will remain the same name.
   *
   * \param kwargs keyword arguments for the symbol
   * \param name name of returned symbol.
   */
  void Compose(const std::unordered_map<std::string, Symbol>& kwargs,
               const std::string& name);
  /*!
   * \brief Convert a list of symbols into static graph
   *
   *  The user can go further to call bind function on static graph
   *
   * \param out_graph the pointer holder of the output graph
   */
  void ToStaticGraph(StaticGraph *out_graph) const;
  /*!
   * \brief Apply the symbol as a function, compose with arguments
   * \param args positional arguments for the symbol
   * \param name name of returned symbol.
   * \return a new Symbol which is the composition of current symbol with its arguments
   */
  Symbol operator () (const std::vector<Symbol>& args, const std::string& name) const;
  /*!
   * \brief compose with named arguments
   * \param kwargs keyword arguments for the symbol
   * \param name name of returned symbol.
   * \return a new symbol which is the composition of current symbol with its arguments
   */
  Symbol operator () (const std::unordered_map<std::string, Symbol>& kwargs,
                      const std::string& name) const;
  /*!
   * \brief infer the shapes of outputs and unknown input arguments
   * \param in_shape the shape of input arguments of the operator
   *     this should be of same length as the vector returned by ListArguments
   *     in_shape allows unknown elements, which are checked by shape.ndim() == 0.
   *     For unknown shapes, InferShape will try to fill in the correct Shape in in_shape
   *     For known shapes, InferShape will check shape consistency
   *
   *     common practice: set the shape of data input, and usually weight's shape can be infered
   *
   * \param out_shape the shape of outputs of the operator
   *     InferShape will modify the vector to fill output TShape
   * \return if the shape inference is successful, return true, else return false.
   */
  bool InferShape(std::vector<TShape> *in_shape, std::vector<TShape> *out_shape) const;
  /*!
   * \brief get number of outputs of this symbol
   * \return number of outputs
   */
  inline size_t NumReturns() const {
    return heads_.size();
  }
  /*!
   * \brief create Symbol by wrapping OperatorProperty
   * This function takes the ownership of op
   *
   * \param op the OperatorProperty of the Operator
   * \return Symbol
   * \sa OperatorProperty::Create
   */
  static Symbol Create(OperatorProperty *op);
  /*!
   * \brief create equivalence of symbol from static graphs
   * \param graph the static graph
   * \return the created symbol
   */
  static Symbol Create(const StaticGraph &graph);
  /*!
   * \brief create equivalence of symbol by grouping the symbols together
   * \param symbols list of symbols
   * \return the grouped symbol
   */
  static Symbol CreateGroup(const std::vector<Symbol> &symbols);
  /*!
   * \brief create variable symbol node
   * \param name name of the variable
   * \return the new variable
   */
  static Symbol CreateVariable(const std::string &name);

 protected:
  // Decalre node, internal data structure.
  struct Node;
  /*! \brief an entry that represents output data from a node */
  struct DataEntry {
    /*! \brief the source node of this data */
    std::shared_ptr<Node> source;
    /*! \brief index of output from the source. */
    uint32_t index;
    /*! \brief enabled default copy constructor */
    DataEntry() {}
    /*! \brief constructor from index */
    DataEntry(std::shared_ptr<Node> source, uint32_t index)
        : source(source), index(index) {}
  };
  /*!
   * \brief the head nodes of Symbols
   * This head is only effective when
   */
  std::vector<DataEntry> heads_;

 private:
  /*! \return whwther the symbol is atomic */
  inline bool is_atomic() const;
  /*!
   * \brief Visit all the nodes in left-to-right depth first order.
   *
   *  This function will visit the graph in DFS order, call fvisit exactly once
   *  for each Node, and store the result in out_result.
   *
   * \param fvisit function applied for each visit.
   * \tparam FVisit visiting function type
   */
  template<typename FVisit>
  inline void DFSVisit(FVisit fvisit) const;
  /*!
   * \brief Find duplicate arguments in the composition
   * \param out the map of argument-name -> occurence count
   * \return maximum number of duplication factor
   */
  int FindDuplicateArgs(std::unordered_map<std::string, int> *out) const;
};

/*!
 * \brief Executor of a computation graph.
 *  Executor can be created by Binding a symbol.
 */
class Executor {
 public:
  /*! \brief destructor */
  virtual ~Executor() {}
  /*!
   * \brief Perform a Forward operation of Operator
   *  After this operation, user can get the result by using function head.
   */
  virtual void Forward() = 0;
  /*!
   * \brief Perform a Backward operation of the Operator.
   *  This must be called after Forward.
   *  After this operation, NArrays specified by grad_in_args_store will be updated accordingly.
   * \param head_grads the gradient of head nodes to be backproped.
   */
  virtual void Backward(const std::vector<NArray> &head_grads) = 0;
  /*!
   * \brief get array of heads in the executor.
   * \return array of heads in the executor.
   */
  virtual const std::vector<NArray> &heads() const = 0;
  /*!
   * \brief Create an operator by bind symbol with context and arguments.
   *  If user do not want to compute the gradients of i-th argument, grad_req_type[i] can be kNullOp.
   *
   * \param ctx the context of binding.
   * \param symbol the symbol that specifies the output of Forward pass.
   * \param in_args the NArray that stores the input arguments to the symbol.
   * \param arg_grad_store NArray that is used to store the gradient output of the input arguments.
   * \param grad_req_type requirment type of gradient saving. Can only be in {kNullOp, kAddTo, kWriteTo}.
   * \return a new executor.
   */
  static Executor *Bind(Symbol symbol,
                        Context ctx,
                        const std::vector<NArray> &in_args,
                        const std::vector<NArray> &arg_grad_store,
                        const std::vector<OpReqType> &grad_req_type);
};  // class operator
}  // namespace mxnet
#endif  // MXNET_SYMBOLIC_H_