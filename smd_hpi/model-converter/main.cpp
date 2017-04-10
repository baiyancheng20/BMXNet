/*!
 * Copyright (c) 2017 by Contributors
 * \file main.cpp
 * \brief model-converter main
 * \author HPI-DeepLearning
*/
#include <stdio.h>
#include <libgen.h>
#include <fstream>

#include <mxnet/ndarray.h>

#include "../src/xnor_cpu.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using mxnet::op::xnor_cpu::BITS_PER_BINARY_WORD;
using mxnet::op::xnor_cpu::BINARY_WORD;


void convert_to_binary(mxnet::NDArray& array) {
  assert(mshadow::mshadow_sizeof(array.dtype()) == sizeof(BINARY_WORD));
  // dim. checks, watch out, second dim is checked to / 32, dont oversee if this really holds for fc
  assert(array.shape().ndim() >= 2); // second dimension is input depth from prev. layer, needed for next line
  assert(array.shape()[1] % BITS_PER_BINARY_WORD == 0); // depth from input has to be divisible by 32 (or 64) since we
  nnvm::TShape binarized_shape(1);
  size_t size = array.shape().Size();
  binarized_shape[0] = size / BITS_PER_BINARY_WORD;
  mxnet::NDArray temp(binarized_shape, mxnet::Context::CPU(), false, array.dtype());
  mxnet::op::xnor_cpu::get_binary_row((float*) array.data().dptr_, (BINARY_WORD*) temp.data().dptr_, size);
  array = temp;
}

int convert_params_file(const std::string& input_file, const std::string& output_file) {
  std::vector<mxnet::NDArray> data;
  std::vector<std::string> keys;

  std::cout << "loading " << input_file << "..." << std::endl;
  { // loading params file into data and keys
    std::unique_ptr<dmlc::Stream> fi(dmlc::Stream::Create(input_file.c_str(), "r"));
    mxnet::NDArray::Load(fi.get(), &data, &keys);
  }

  //const std::string filter_strings();
  const std::vector<std::string> filter_strings {"qconvolution", "qfullyconnected"};


  auto containsFilterString = [filter_strings](std::string line_in_params) {
    auto containsSubString = [line_in_params](std::string filter_string) {
      return line_in_params.find(filter_string) != std::string::npos;};
    return std::find_if(filter_strings.begin(),
                        filter_strings.end(),
                        containsSubString) != filter_strings.end();};

  auto iter = std::find_if(keys.begin(),
                           keys.end(),
                           containsFilterString);

  //Use a while loop, checking whether iter is at the end of myVector
  //Do a find_if starting at the item after iter, std::next(iter)
  while (iter != keys.end())
  {
    std::cout << "|- converting weights " << *iter << "..." << std::endl;
    CHECK((*iter).find("weight") != std::string::npos) << "onyl weight binarization supported currently";

    convert_to_binary(data[iter - keys.begin()]);

    iter = std::find_if(std::next(iter),
                        keys.end(),
                        containsFilterString);
  }


  { // saving params back to *_converted
    std::unique_ptr<dmlc::Stream> fo(dmlc::Stream::Create(output_file.c_str(), "w"));
    mxnet::NDArray::Save(fo.get(), data, keys);
  }
  std::cout << "wrote converted params to " << output_file << std::endl;
  return 0;
}

int convert_json_file(const std::string& input_fname, const std::string& output_fname) {
  std::cout << "loading " << input_fname << "..." << std::endl;
  std::string json;
  {
    std::ifstream stream(input_fname);
    if (!stream.is_open()) {
      std::cout << "cant find json file at " + input_fname << std::endl;
      return -1;
    }
    std::stringstream buffer;
    buffer << stream.rdbuf();
    json = buffer.str();
  }

  rapidjson::Document d;
  d.Parse(json.c_str());

  assert(d.HasMember("nodes"));
  rapidjson::Value& nodes = d["nodes"];
  assert(nodes.IsArray());

  for (rapidjson::Value::ValueIterator itr = nodes.Begin(); itr != nodes.End(); ++itr) {
    if (!(itr->HasMember("op") && (*itr)["op"].IsString() &&
            (std::strcmp((*itr)["op"].GetString(), "QConvolution") == 0 ||
             std::strcmp((*itr)["op"].GetString(), "QFullyConnected") == 0))) {
      continue;
    }

    assert((*itr).HasMember("attr"));
    rapidjson::Value& op_attributes = (*itr)["attr"];
    op_attributes.AddMember("binarized_weights_only", "True", d.GetAllocator());

    assert((*itr).HasMember("name"));
    std::cout << "|- adjusting attributes for " << (*itr)["name"].GetString() << std::endl;
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  d.Accept(writer);

  {
    std::ofstream stream(output_fname);
    if (!stream.is_open()) {
      std::cout << "cant find json file at " + output_fname << std::endl;
      return -1;
    }
    std::string output = buffer.GetString();
    stream << output;
    stream.close();
  }

  std::cout << "wrote converted json to " << output_fname << std::endl;

  return 0;
}

int main(int argc, char ** argv){
  if (argc != 2) {
    std::cout << "usage: " + std::string(argv[0]) + " <mxnet *.params file>" << std::endl;
    std::cout << "  will binarize the weights of the Convolutional Layers of your model," << std::endl;
    std::cout << "  pack 32 values into one and save the result with the prefix 'binarized_'" << std::endl;
    return -1;
  }

  const std::string params_file(argv[1]);

  const std::string path(dirname(argv[1]));
  const std::string params_file_name(basename(argv[1]));
  std::string base_name = params_file_name;
  base_name.erase(base_name.rfind('-')); // watchout if no '-'
  const std::string output_name(path + "/" + "binarized_" + params_file_name);

  if (convert_params_file(params_file, output_name) != 0) {
    return -1;
  }

  const std::string json_file_name(path + "/"                + base_name + "-symbol.json");
  const std::string json_out_fname(path + "/" + "binarized_" + base_name + "-symbol.json");

  if (convert_json_file(json_file_name, json_out_fname) != 0) {
    return -1;
  }

  return 0;
}