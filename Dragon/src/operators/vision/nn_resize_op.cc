#include "operators/vision/nn_resize_op.h"
#include "core/workspace.h"
#include "utils/math_functions.h"
#include "utils/op_kernel.h"

namespace dragon {

template <class Context> template <typename T>
void NNResizeOp<Context>::RunWithType() {
    auto* Xdata = input(0).template data<T, Context>();
    auto* Ydata = output(0)->template mutable_data<T, Context>();
    kernel::NNResize<T, Context>(output(0)->count(), dims[0], dims[1],
                                     input(0).dim(2), input(0).dim(3),
                                                     dims[2], dims[3],
                                                                Xdata,
                                                               Ydata);
}

template <class Context>
void NNResizeOp<Context>::RunOnDevice() {
    dims = input(0).dims();
    if (dynamic_dsize.size() > 0) {
        CHECK_EQ(dynamic_dsize.size(), 2)
            << "\nThe dsize should be a scalar with 2 elements.";
        for (int i = 0; i < 2; i++) {
            Tensor* t = ws()->GetTensor(dynamic_dsize[i]);
            if (t->IsType<int>()) {
                dims[2 + i] = t->template data<int, CPUContext>()[0];
            } else if (t->IsType<float>()) {
                dims[2 + i] = t->template data<float, CPUContext>()[0];
            } else {
                LOG(FATAL) << "Unsupported types of dsize.";
            }
        }
    } else if (static_dsize.size() > 0) {
        CHECK_EQ(static_dsize.size(), 2)
            << "\nThe dsize should be a scalar with 2 elements.";
        for (int i = 0; i < 2; i++) dims[2 + i] = static_dsize[i];
    } else {
        CHECK(fy != -1.0 && fx != -1.0)
            << "\nThe fx and fy should be set.";
        dims[2] = int(dims[2] * fy);
        dims[3] = int(dims[3] * fx);
    }
    output(0)->Reshape(dims);
    
    if (input(0).template IsType<float>()) return RunWithType<float>();
    else LOG(FATAL) << "Unsupported input types.";
}

DEPLOY_CPU(NNResize);
#ifdef WITH_CUDA
DEPLOY_CUDA(NNResize);
#endif
OPERATOR_SCHEMA(NNResize).NumInputs(1).NumOutputs(1);

template <class Context> template <typename T>
void NNResizeGradientOp<Context>::RunWithType() {
    auto* dYdata = input(-1).template data<T, Context>();
    auto* dXdata = output(0)->template mutable_data<T, Context>();
    math::Set<T, Context>(output(0)->count(), 0, dXdata);
    kernel::NNResizeGrad<T, Context>(input(-1).count(), input(0).dim(0), input(0).dim(1),
                                                      input(-1).dim(2), input(-1).dim(3),
                                                    output(0)->dim(2), output(0)->dim(3),
                                                                                  dYdata,
                                                                                 dXdata);
}

template <class Context>
void NNResizeGradientOp<Context>::RunOnDevice() {
    output(0)->ReshapeLike(input(0));
    
    if (input(0).template IsType<float>()) return RunWithType<float>();
    else LOG(FATAL) << "Unsupported input types.";
}

DEPLOY_CPU(NNResizeGradient);
#ifdef WITH_CUDA
DEPLOY_CUDA(NNResizeGradient);
#endif
OPERATOR_SCHEMA(NNResizeGradient).NumInputs(2).NumOutputs(1);

class GetNNResizeGradient final : public GradientMakerBase {
 public:
    GRADIENT_MAKER_CTOR(GetNNResizeGradient);
    vector<OperatorDef> MakeDefs() override {
        return SingleDef(def.type() + "Gradient", "",
            vector<string> {I(0), GO(0)},
            vector<string> {GI(0)});
    }
};
REGISTER_GRADIENT(NNResize, GetNNResizeGradient);

}    // namespace dragon