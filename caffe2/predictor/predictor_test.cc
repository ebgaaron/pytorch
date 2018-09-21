#include "caffe2/core/context.h"
#include "caffe2/core/operator.h"
#include "caffe2/core/tensor.h"
#include "caffe2/predictor/predictor.h"
#include "caffe2/utils/math.h"

#include <gtest/gtest.h>

namespace caffe2 {

namespace {

const char* predictSpec = R"DOC(
        name: "predict"
        type: "dag"
        external_input: "data"
        external_input: "W"
        external_input: "b"
        external_output: "y"
        op {
          input: "data"
          input: "W"
          input: "b"
          output: "y"
          type: "FC"
        }
)DOC";

const char* initSpec = R"DOC(
        name: "init"
        type: "dag"
        op {
          type: "ConstantFill"
          output: "W"
          arg {
            name: "shape"
            ints: 10
            ints: 4
          }
          arg {
            name: "value"
            f: 2.0
          }
        }
        op {
          type: "ConstantFill"
          output: "b"
          arg {
            name: "shape"
            ints: 10
          }
          arg {
            name: "value"
            f: 2.0
          }
        }

)DOC";

const char* metaSpec = R"DOC(
  blobs {
    key: "INPUTS_BLOB_TYPE"
    value: "data"
  }
  blobs {
      key: "OUTPUTS_BLOB_TYPE"
      value: "y"
  }
  nets {
    key: "GLOBAL_INIT_NET_TYPE"
    value: {
      name: "init"
      type: "dag"
      op {
        type: "ConstantFill"
        output: "data"
        arg {
          name: "shape"
          ints: 1
          ints: 4
        }
        arg {
          name: "value"
          f: 2.0
        }
      }
      op {
        type: "ConstantFill"
        output: "W"
        arg {
          name: "shape"
          ints: 10
          ints: 4
        }
        arg {
          name: "value"
          f: 2.0
        }
      }
      op {
        type: "ConstantFill"
        output: "b"
        arg {
          name: "shape"
          ints: 10
        }
        arg {
          name: "value"
          f: 2.0
        }
      }
    }
  }
  nets {
    key: "PREDICT_NET_TYPE"
    value: {
      name: "predict"
      type: "dag"
      external_input: "data"
      external_input: "W"
      external_input: "b"
      external_output: "y"
      op {
        input: "data"
        input: "W"
        input: "b"
        output: "y"
        type: "FC"
      }
    }
  }
)DOC";

std::unique_ptr<Blob> randomTensor(
    const std::vector<TIndex>& dims,
    CPUContext* ctx) {
  auto blob = make_unique<Blob>();
  auto* t = BlobGetMutableTensor(blob.get(), CPU);
  t->Resize(dims);
  math::RandUniform<float, CPUContext>(
      t->size(), -1.0, 1.0, t->template mutable_data<float>(), ctx);
  return blob;
}

NetDef parseNetDef(const std::string& value) {
  NetDef def;
  CAFFE_ENFORCE(
      TextFormat::ParseFromString(value, &def),
      "Failed to parse NetDef with value: ",
      value);
  return def;
};

MetaNetDef parseMetaNetDef(const std::string& value) {
  MetaNetDef def;
  CAFFE_ENFORCE(
      TextFormat::ParseFromString(value, &def),
      "Failed to parse NetDef with value: ",
      value);
  return def;
}
}

class PredictorTest : public testing::Test {
 public:
  void SetUp() override {
    DeviceOption op;
    op.set_random_seed(1701);
    ctx_ = caffe2::make_unique<CPUContext>(op);
    NetDef init, run;
    p_ = caffe2::make_unique<Predictor>(
        makePredictorConfig(parseNetDef(initSpec), parseNetDef(predictSpec)));
  }

  std::unique_ptr<CPUContext> ctx_;
  std::unique_ptr<Predictor> p_;
};

TEST_F(PredictorTest, SimpleBatchSized) {
  auto inputData = randomTensor({1, 4}, ctx_.get());
  Predictor::TensorList input;
  input.emplace_back(CPU);
  auto tensor = BlobGetMutableTensor(inputData.get(), CPU);
  input.back().ResizeLike(*tensor);
  input.back().ShareData(*tensor);
  Predictor::TensorList output;
  (*p_)(input, &output);
  EXPECT_EQ(output.size(), 1);
  EXPECT_EQ(output.front().dims().size(), 2);
  EXPECT_EQ(output.front().dim(0), 1);
  EXPECT_EQ(output.front().dim(1), 10);
  EXPECT_NEAR(output.front().data<float>()[4], 0.1209, 1E-4);
}

TEST_F(PredictorTest, SimpleBatchSizedMapInput) {
  auto inputData = randomTensor({1, 4}, ctx_.get());
  Predictor::TensorMap input;
  auto iter = input.emplace("data", Tensor(CPU));
  auto tensor = BlobGetMutableTensor(inputData.get(), CPU);
  iter.first->second.ResizeLike(*tensor);
  iter.first->second.ShareData(*tensor);

  Predictor::TensorList output;
  (*p_)(input, &output);
  EXPECT_EQ(output.size(), 1);
  EXPECT_EQ(output.front().dims().size(), 2);
  EXPECT_EQ(output.front().dim(0), 1);
  EXPECT_EQ(output.front().dim(1), 10);
  EXPECT_NEAR(output.front().data<float>()[4], 0.1209, 1E-4);
}

} // namespace caffe2
