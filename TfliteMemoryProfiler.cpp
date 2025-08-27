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

// 메모리 접근 유형을 구분
enum AccessType {
  INPUT = 0,
  OUTPUT = 1,
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


PreservedAnalyses TfliteProfilerPass::run(Function& F, 
	FunctionAnalysisManager & FAM) {
	StringRef functionName = F.getName();
	if (!functionName.contains("FullyConnected")) {
		return PreservedAnalyses::all();
	}

	// 함수의 인자 저장
	SmallVector<Argument*, 8> Args;
	for (auto& Arg : F.args()) {
		Args.push_back(&Arg);
	}

	// 함수 시그니처와 맞는지 확인 가능 시 일반화 코드로 변경 필요
	// float인 경우에 대해서만 test code
	if (Args.size() != 9)
			return PreservedAnalyses::all();
	
	Argument* input_data_arg = Args[2];  
	Argument* output_data_arg = Args[8]; 

	// IR에 삽입할 로깅 함수 선언
	LLVMContext& Ctx = F.getContext();
	Module* module = F.getParent();
	FunctionCallee logFunc = module->getOrInsertFunction(
			"log_mem_access", Type::getVoidTy(Ctx), Type::getInt8Ty(Ctx),
			Type::getInt32Ty(Ctx));

	bool modified = false;

	for (auto& BB : F) {
		for (auto& I : BB) {
			Value* accessPtr = nullptr;
			Argument* originArg = nullptr;
			AccessType accessType;

			if (LoadInst* loadInst = dyn_cast<LoadInst>(&I)) {
				// Load 명령어의 경우, input_data에서의 읽기인지 확인
				accessPtr = loadInst->getPointerOperand();
				Value* origin = tracePointerOrigin(accessPtr);
				if (origin == input_data_arg) {
					originArg = input_data_arg;
					accessType = INPUT;
				}
			} else if (StoreInst* storeInst = dyn_cast<StoreInst>(&I)) {
				// Store 명령어의 경우, output_data에 대한 쓰기인지 확인
				accessPtr = storeInst->getPointerOperand();
				Value* origin = tracePointerOrigin(accessPtr);
				if (origin == output_data_arg) {
					originArg = output_data_arg;
					accessType = OUTPUT;
				}
			}

			// 타겟 포인터에 대한 접근일 경우, 로깅 함수 호출 코드 삽입
			if (originArg) {
				IRBuilder<> builder(&I);
				Value* address = builder.CreateBitCast(accessPtr, llvm::PointerType::get(Type::getInt8Ty(Ctx), 0));
				Value* type = ConstantInt::get(Type::getInt32Ty(Ctx), accessType);
				builder.CreateCall(logFunc, {address, type});
				modified = true;
			}
		}
	}
	// return modified;
	return PreservedAnalyses::none();
}



extern "C" PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "MemoryProfilerPassPlugin",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            dbgs() << "[DEBUG] Registering MemoryProfiler Pass";
            PB.registerPipelineParsingCallback(
                [](const StringRef name, FunctionPassManager& FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                        if (name == "tflite-memory-profiler") {
                            FPM.addPass(TfliteProfilerPass());
                            return true;
                        } else {
                            return false;
                        }
                    }
            );
        }
    };
}