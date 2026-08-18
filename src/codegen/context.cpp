#include "context.hpp"
#include "common.hpp"

#include <ranges>
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h" // InstCombinePass
#include "llvm/Transforms/Scalar/Reassociate.h"     // ReassociatePass
#include "llvm/Transforms/Scalar/GVN.h"             // GVNPass
#include "llvm/Transforms/Scalar/SimplifyCFG.h"     // SimplifyCFGPass
#include "llvm/Transforms/Utils/Mem2Reg.h"         // PromotePass (mem2reg)

CodeGenContext::CodeGenContext() {
    TheContext = std::make_unique<llvm::LLVMContext>();
    TheModule = std::make_unique<llvm::Module>("code.txt", *TheContext);
    Builder = std::make_unique<llvm::IRBuilder<> >(*TheContext);

    TheFPM = std::make_unique<llvm::FunctionPassManager>();
    TheMAM = std::make_unique<llvm::ModuleAnalysisManager>();
    TheFAM = std::make_unique<llvm::FunctionAnalysisManager>();
    TheLAM = std::make_unique<llvm::LoopAnalysisManager>();
    TheCGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
    ThePIC = std::make_unique<llvm::PassInstrumentationCallbacks>();
    TheSI = std::make_unique<llvm::StandardInstrumentations>(*TheContext,/*debugging*/true);

    TheSI->registerCallbacks(*ThePIC, TheMAM.get());

    // Register analysis passes
    llvm::PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.registerLoopAnalyses(*TheLAM);
    PB.registerCGSCCAnalyses(*TheCGAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);

    // 添加kaleidoscope教程中给的一些优化pass
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    TheFPM->addPass(llvm::InstCombinePass());
    // Reassociate expressions.
    TheFPM->addPass(llvm::ReassociatePass());
    // Eliminate Common SubExpressions.
    TheFPM->addPass(llvm::GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    TheFPM->addPass(llvm::SimplifyCFGPass());

    // 添加mem2reg的优化pass
    TheFPM->addPass(llvm::PromotePass());

    // 添加目标二进制文件生成的配置
    // Initialize the target registry etc.
    // 不预先初始化进行注册的话，后续的lookupTarget会无法获取到。
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
    const auto CPU = "generic"; // 通用CPU
    const auto Features = ""; // 不额外指定feature
    llvm::TargetOptions opt; // 默认
    std::string Error;

    const auto TargetTriple = llvm::Triple(llvm::sys::getDefaultTargetTriple());
    debug("TargetTriple: ");
    debug(llvm::sys::getDefaultTargetTriple().c_str());
    debug('\n');
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);
    if (Target == nullptr) {
        std::cerr << Error << std::endl;
        exit(1);
    }
    TheTM.reset(Target->createTargetMachine(
        TargetTriple, CPU, Features, opt, llvm::Reloc::PIC_));

    TheModule->setTargetTriple(TargetTriple);
    TheModule->setDataLayout(TheTM->createDataLayout());
    TheModule->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                             llvm::DEBUG_METADATA_VERSION);
    TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);

    // debug信息创建
    TheDIBuilder = std::make_unique<llvm::DIBuilder>(*TheModule);
    TheCU = TheDIBuilder->createCompileUnit(
        llvm::dwarf::DW_LANG_C,
        TheDIBuilder->createFile("code.txt", "."),
        "Kaleidoscope Compiler",
        false,
        "",
        0
    );
    doubleType = TheDIBuilder->createBasicType(
        "double",
        64,
        llvm::dwarf::DW_ATE_float);
}

void CodeGenContext::genObjFile(std::string fileName) {
    llvm::verifyModule(*TheModule);

    std::error_code EC;
    llvm::raw_fd_ostream dest(fileName, EC, llvm::sys::fs::OF_None);

    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        exit(1);
    }

    llvm::legacy::PassManager pass;
    auto FileType = llvm::CodeGenFileType::ObjectFile;
    if (TheTM->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        llvm::errs() << "TheTargetMachine can't emit a file of this type";
        exit(1);
    }

    pass.run(*TheModule);
    dest.flush();
    llvm::errs() << "Wrote " << fileName << "\n";
}


llvm::Value *CodeGenContext::LookupName(const std::string &Name) {
    for (const auto &nameValues: std::ranges::reverse_view(NamedValuesStack)) {
        if (const auto it = nameValues.find(Name); it != nameValues.end()) {
            return it->second;
        }
    }
    return nullptr;
}


llvm::AllocaInst *CodeGenContext::allocaVar(llvm::Function &fun, const std::string &name) {
    auto &cur_scope = NamedValuesStack.back();
    // 检查是否重名
    if (const auto it = cur_scope.find(name); it != cur_scope.end()) {
        return nullptr;
    }
    // 在函数开头插入alloca
    auto tempB = llvm::IRBuilder<>(&fun.getEntryBlock(), fun.getEntryBlock().begin());
    cur_scope[name] = tempB.CreateAlloca(llvm::Type::getDoubleTy(*TheContext), nullptr, name);
    return cur_scope[name];
}

void CodeGenContext::emitLocation(ExprAst *ast) {
    if (!ast) {
        Builder->SetCurrentDebugLocation(llvm::DebugLoc());
        return;
    }
    llvm::DIScope *scope;
    if (lexicalBlocks.empty()) {
        scope = TheCU;
    } else {
        scope = lexicalBlocks.back();
    }
    const auto pos = ast->getLocation();
    Builder->SetCurrentDebugLocation(llvm::DILocation::get(
        scope->getContext(),
        pos.line,
        pos.column,
        scope
    ));
}
