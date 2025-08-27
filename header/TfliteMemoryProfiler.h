#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"

using namespace llvm;
struct TfliteProfilerPass : public PassInfoMixin<TfliteProfilerPass> {
public:
    static PreservedAnalyses run(Function &F, 
        FunctionAnalysisManager &FAM);
};