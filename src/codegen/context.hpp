#ifndef PRACTICE_KALEIDOSCOPE_CONTEXT_HPP
#define PRACTICE_KALEIDOSCOPE_CONTEXT_HPP
#include <memory>
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Target/TargetMachine.h"

#include "parser/ast.hpp"


class CodeGenContext {
public:
    // 核心 LLVM 对象
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::Module> TheModule;
    std::unique_ptr<llvm::IRBuilder<> > Builder;

    // LLVM 后端对象
    std::unique_ptr<llvm::TargetMachine> TheTM;

    // llvm pass 对象
    std::unique_ptr<llvm::FunctionPassManager> TheFPM;

    std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
    std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
    std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
    std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
    std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
    std::unique_ptr<llvm::StandardInstrumentations> TheSI;

    // 符号表
    std::vector<std::map<std::string, llvm::AllocaInst *> > NamedValuesStack;

    // 二元算符运算表
    std::map<char, std::function<llvm::Value*(llvm::Value *, llvm::Value *, CodeGenContext &)> > BinOpMap;

    // debug 信息生成所需
    llvm::DICompileUnit *TheCU;
    std::unique_ptr<llvm::DIBuilder> TheDIBuilder;
    std::vector<llvm::DIScope *> lexicalBlocks = {};
    //唯一在用的double类型
    llvm::DIType *doubleType;

    CodeGenContext();

    void genObjFile(std::string fileName);

    void EnterScope(llvm::DIScope *scope) {
        NamedValuesStack.emplace_back();
        lexicalBlocks.push_back(scope);
    }

    void ExitScope() {
        NamedValuesStack.pop_back();
        lexicalBlocks.pop_back();
    }

    void setBinOp(const char op,
                  const std::function<llvm::Value *(llvm::Value *, llvm::Value *, CodeGenContext &)> &fn) {
        BinOpMap[op] = fn;
    }

    llvm::AllocaInst *allocaVar(llvm::Function &fun, const std::string &name);

    llvm::Value *LookupName(const std::string &Name);

    void emitLocation(ExprAst *ast);
};

#endif //PRACTICE_KALEIDOSCOPE_CONTEXT_HPP
