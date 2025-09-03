#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/Support/raw_ostream.h"

#include "header/TfliteMemoryProfiler.h"
using namespace llvm;

#include "llvm/Passes/OptimizationLevel.h"

// 메모리 접근 유형을 구분
enum AccessType {
  LOAD = 0,
  STORE = 1,
  ALLOC_STACK = 2
};

// 포인터의 베이스 포인터을 찾음
Value* tracePointerOrigin(Value* Ptr) {
  // GetElementPtrInst (GEP)는 주소의 오프셋 계산 명령어
  // GEP의 체인을 거슬러 올라가 최초의 포인터를 찾아 OUTPUT, INPUT배열이 맞는지 확인
  while (auto* GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
    Ptr = GEP->getPointerOperand();
  }
  return Ptr;
}


PreservedAnalyses TfliteProfilerPass::run(Module& M, 
	ModuleAnalysisManager & MAM) {

	LLVMContext& Context = M.getContext();
        const DataLayout &Layout = M.getDataLayout();

        std::vector<Type*> ArgTys;
        ArgTys.push_back(llvm::PointerType::get(Type::getInt8Ty(Context), 0));
        ArgTys.push_back(Type::getInt32Ty(Context));
        //ArgTys.push_back(M.getDataLayout().getIntPtrType(Context));         // size_t (load 크기)
        FunctionType* traceFuncTy = FunctionType::get(
        	Type::getVoidTy(Context), ArgTys, /*isVarArg=*/false);

	FunctionCallee logFunc = M.getOrInsertFunction(
			"logMemAccess",
			traceFuncTy);

	for (Function& F : M) {
        StringRef N = F.getName();
        if (N == "logMemAccess") {
            errs() << "[Profiler] skipping instrumentation of " << F.getName() << "\n";
            continue;
        }
        if (N.contains("PrintAllocations") ||
            N.contains("LogTicksPerTagCsv") ||
            N.contains("RecordingMicroAllocator") ||
            N.contains("MicroProfiler") ||
            N.contains("Profiler"))
          continue;
		for (auto& BB : F) {
			for (auto& I : BB) {
				if (LoadInst* loadInst = dyn_cast<LoadInst>(&I)) {
                    IRBuilder<> builder(loadInst);

					Value* accessPtr = loadInst->getPointerOperand();
                    Value* address = builder.CreateBitCast(accessPtr, llvm::PointerType::get(Type::getInt8Ty(Context), 0));
					Value* origin = tracePointerOrigin(accessPtr);
                    Value* type = ConstantInt::get(Type::getInt32Ty(Context), LOAD);

					builder.CreateCall(logFunc, {address, type /*, loadedSize */});
				} else if (StoreInst* storeInst = dyn_cast<StoreInst>(&I)) {
                    IRBuilder<> builder(storeInst);

					Value* accessPtr = storeInst->getPointerOperand();
                    Value* address = builder.CreateBitCast(accessPtr, llvm::PointerType::get(Type::getInt8Ty(Context), 0));
					Value* origin = tracePointerOrigin(accessPtr);
                    Value* type = ConstantInt::get(Type::getInt32Ty(Context), STORE);

                    builder.CreateCall(logFunc, {address, type});
				}
			}
		}
	}
	// return modified;
	return PreservedAnalyses::none();
}



extern "C" PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "TfliteProfilerPassPlugin",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            dbgs() << "[DEBUG] Registering MemoryProfiler Pass";
//            PB.registerPipelineParsingCallback(
//                [](const StringRef name, ModulePassManager& MPM,
//                    ArrayRef<PassBuilder::PipelineElement> Pipeline) {
//                        if (name == "tflite-memory-profiler") {
//                            MPM.addPass(TfliteProfilerPass());
//                            return true;
//                        } else {
//                            return false;
//                        }
//                    }
//            );
//            PB.registerPipelineStartEPCallback(
//            [&](ModulePassManager &MPM, OptimizationLevel level) {
//              MPM.addPass(TfliteProfilerPass());
//              dbgs() << "[DEBUG] Inserted TfliteProfilerPass at pipeline start\n";
//            });
            PB.registerOptimizerLastEPCallback(
            [&](ModulePassManager &MPM,
                OptimizationLevel Level,
                ThinOrFullLTOPhase Phase) {
              MPM.addPass(TfliteProfilerPass());
            });
        }
    };
}
