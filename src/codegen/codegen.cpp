#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"

#include "parser/ast.hpp"
#include "context.hpp"


llvm::Value *DefinitionAst::gen_code(CodeGenContext &ctx) {
    // 如果是binary操作符自定义函数，需要注册到BinOpMap中
    // 提前定义，确保递归定义时，可以正常生成。
    if (this->prototype.precedence.has_value()) {
        std::string name = this->prototype.identifier.name;
        char op = name.back();
        ctx.setBinOp(op, [name](auto l, auto r, CodeGenContext &ctx) {
            auto fun = ctx.TheModule->getFunction(name);
            if (!fun) {
                throw std::runtime_error{"Unknow error for lack binary definition "};
            }
            return static_cast<llvm::Value *>(ctx.Builder->CreateCall(fun, std::vector{l, r}));
        });
    }

    auto fun = ctx.TheModule->getFunction(this->prototype.identifier.name);
    if (!fun) {
        fun = llvm::dyn_cast<llvm::Function>(this->prototype.gen_code(ctx));
    } else if (fun->arg_size() != this->prototype.args.size()) {
        throw std::runtime_error{"参数不匹配"};
    } else if (!fun->empty()) {
        throw std::runtime_error{"重复定义"};
    }
    fun->addFnAttr("frame-pointer", "all");
    // debug function信息
    llvm::DIFile *fileScope = ctx.TheCU->getFile();
    llvm::DIType *ret = ctx.doubleType;
    std::vector<llvm::Metadata *> funTypes = {ret};
    for (const auto &arg: fun->args()) {
        funTypes.emplace_back(ctx.doubleType);
    }
    const auto funTy = ctx.TheDIBuilder->createSubroutineType(
        ctx.TheDIBuilder->getOrCreateTypeArray(funTypes));
    llvm::DISubprogram *subprogram = ctx.TheDIBuilder->createFunction(
        fileScope,
        this->prototype.identifier.name,
        llvm::StringRef(),
        fileScope,
        this->prototype.identifier.loc.start.line,
        funTy,
        this->prototype.identifier.loc.start.line,
        llvm::DINode::FlagPrototyped,
        llvm::DISubprogram::SPFlagDefinition
    );
    fun->setSubprogram(subprogram);

    // 开始创建ir
    ctx.EnterScope(subprogram);

    auto enter_block = llvm::BasicBlock::Create(*ctx.TheContext, "entry", fun);
    ctx.Builder->SetInsertPoint(enter_block);

    ctx.emitLocation(nullptr); // clear 行信息，因为这些alloca指令是不存在源码对应的。
    unsigned argIdx = 1; // argNo从1开始
    for (auto &arg: fun->args()) {
        // 把函数的参数的值放到变量空间中
        const auto argLineNo = this->prototype.args[argIdx - 1].loc.start.line;
        const auto argColumn = this->prototype.args[argIdx - 1].loc.start.column;
        const auto arg_ptr = ctx.allocaVar(*fun, std::string(arg.getName()));
        auto D = ctx.TheDIBuilder->createParameterVariable(
            subprogram,
            arg.getName(),
            argIdx,
            fileScope,
            argLineNo,
            ctx.doubleType,
            true
        );
        // 插入declare指令，这个需要注意插入的位置了。
        ctx.TheDIBuilder->insertDeclare(arg_ptr, D,
                                        ctx.TheDIBuilder->createExpression(),
                                        llvm::DILocation::get(subprogram->getContext(),
                                                              argLineNo,
                                                              argColumn,
                                                              subprogram),
                                        ctx.Builder->GetInsertBlock());
        ctx.Builder->CreateStore(&arg, arg_ptr);
        argIdx++;
    }
    // 发出地址，目的只是在后续生产代码时未指定的位置的情况下能够自动继承。
    // 不适合在生成之后再使用，因为这个可能导致行号增加之后，突然的回退跳跃。
    ctx.emitLocation(this->body.get());
    llvm::Value *retValue = this->body->gen_code(ctx);
    ctx.Builder->CreateRet(retValue);
    llvm::verifyFunction(*fun);
    // 调用pass进行优化
    if (std::getenv("DISABLE_OPT") == nullptr)
        ctx.TheFPM->run(*fun, *ctx.TheFAM);
    ctx.ExitScope();
    // 结束创建ir

    return fun;
}

llvm::Value *ExternAst::gen_code(CodeGenContext &ctx) {
    return this->prototype.gen_code(ctx);
}


llvm::Value *PrototypeAst::gen_code(CodeGenContext &ctx) {
    // prototype only : double(double...)
    std::vector args_type{this->args.size(), llvm::Type::getDoubleTy(*ctx.TheContext)};
    auto fun_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx.TheContext), args_type, false);
    auto fun = llvm::Function::Create(fun_type,
                                      llvm::Function::ExternalLinkage,
                                      ctx.TheModule->getDataLayout().getProgramAddressSpace(),
                                      this->identifier.name,
                                      ctx.TheModule.get());
    unsigned Idx = 0;
    for (auto &arg: fun->args())
        arg.setName(this->args[Idx++].name);

    return fun;
}

llvm::Value *NumberExprAst::gen_code(CodeGenContext &ctx) {
    ctx.emitLocation(this);
    return llvm::ConstantFP::get(*ctx.TheContext, llvm::APFloat(this->number.getValue()));
}


llvm::Value *VariableExprAst::gen_code(CodeGenContext &ctx) {
    auto v = ctx.LookupName(identifier.name);
    if (!v) {
        throw std::runtime_error{"引用变量不存在"};
    }
    ctx.emitLocation(this);
    return ctx.Builder->CreateLoad(llvm::Type::getDoubleTy(*ctx.TheContext), v);
}


llvm::Value *BinaryExprAst::gen_code(CodeGenContext &ctx) {
    if (this->ops == '=') {
        if (const auto left = dynamic_cast<VariableExprAst *>(this->left.get())) {
            auto l = ctx.LookupName(left->identifier.name);
            auto r = right->gen_code(ctx);
            ctx.emitLocation(this);
            ctx.BinOpMap[ops](l, r, ctx);
            return r;
        }
        throw std::runtime_error{"left of '=' should be a variable."};
    }
    auto l = left->gen_code(ctx);
    auto r = right->gen_code(ctx);
    if (!l || !r) {
        throw std::runtime_error{"binary lacked."};
    }
    if (ctx.BinOpMap[ops]) {
        ctx.emitLocation(this);
        return ctx.BinOpMap[ops](l, r, ctx);
    }
    throw std::runtime_error{"binary gen failed."};
}

llvm::Value *UnaryExprAst::gen_code(CodeGenContext &ctx) {
    const auto funName = std::string("unary") + this->ops.value;
    const auto fun = ctx.TheModule->getFunction(funName);
    if (!fun) {
        throw std::runtime_error{"Unknown unary referenced"};
    }
    const auto arg = this->operand->gen_code(ctx);
    ctx.emitLocation(this);
    return ctx.Builder->CreateCall(fun, arg, "unarycall");
}

llvm::Value *IfExprAst::gen_code(CodeGenContext &ctx) {
    // entry, for condition
    ctx.emitLocation(this); // 针对condition的位置
    auto condV = this->condition->gen_code(ctx);
    condV = ctx.Builder->CreateFCmpONE(condV, llvm::ConstantFP::get(*ctx.TheContext, llvm::APFloat(0.0)), "ifcond");
    // 这里就先这么写了，下一个练手项目再做好scope的管理。
    auto fun = ctx.Builder->GetInsertBlock()->getParent();
    auto thenBB = llvm::BasicBlock::Create(*ctx.TheContext, "thenBB");
    auto elseBB = llvm::BasicBlock::Create(*ctx.TheContext, "elseBB");
    auto mergeBB = llvm::BasicBlock::Create(*ctx.TheContext, "ifcont");

    ctx.Builder->CreateCondBr(condV, thenBB, elseBB);

    //then basic block
    fun->insert(fun->end(), thenBB);
    ctx.Builder->SetInsertPoint(thenBB);
    auto thenV = this->thenExpr->gen_code(ctx);
    ctx.Builder->CreateBr(mergeBB);
    // because thenExpr gencode may change the basic block
    auto thenBBOut = ctx.Builder->GetInsertBlock();

    // else basic block
    fun->insert(fun->end(), elseBB);
    ctx.Builder->SetInsertPoint(elseBB);
    auto elseV = this->elseExpr->gen_code(ctx);
    ctx.Builder->CreateBr(mergeBB);
    auto elseBBOut = ctx.Builder->GetInsertBlock();

    // if merge out
    fun->insert(fun->end(), mergeBB);
    ctx.Builder->SetInsertPoint(mergeBB);
    auto pn = ctx.Builder->CreatePHI(llvm::Type::getDoubleTy(*ctx.TheContext), 2, "iftemp");
    pn->addIncoming(thenV, thenBBOut);
    pn->addIncoming(elseV, elseBBOut);
    return pn;
}

llvm::Value *ForExprAst::gen_code(CodeGenContext &ctx) {
    auto fun = ctx.Builder->GetInsertBlock()->getParent();
    auto preLoopBB = ctx.Builder->GetInsertBlock();
    auto loopBB = llvm::BasicBlock::Create(*ctx.TheContext, "loopBB");
    auto loopEndBB = llvm::BasicBlock::Create(*ctx.TheContext, "loopEndBB");
    // for的debug信息
    llvm::DIScope *parentSP;
    if (ctx.lexicalBlocks.empty()) {
        parentSP = ctx.TheCU;
    } else {
        parentSP = ctx.lexicalBlocks.back();
    }
    llvm::DIFile *file = ctx.TheDIBuilder->createFile(ctx.TheCU->getFilename(),
                                                      ctx.TheCU->getDirectory());
    const auto sp = ctx.TheDIBuilder->createLexicalBlock(
        parentSP,
        file,
        this->getLocation().line,
        this->getLocation().column);

    // 开始ir生成
    ctx.EnterScope(sp);
    ctx.emitLocation(this);

    // declare var
    if (!ctx.allocaVar(*fun, this->var.name)) {
        throw std::runtime_error{"重复的var."};
    }
    const auto loopVar = ctx.LookupName(this->var.name);
    const auto D = ctx.TheDIBuilder->createAutoVariable(
        sp,
        var.name,
        file,
        var.loc.start.line,
        ctx.doubleType,
        true);
    ctx.TheDIBuilder->insertDeclare(
        loopVar,
        D,
        ctx.TheDIBuilder->createExpression(),
        llvm::DILocation::get(sp->getContext(),
                              var.loc.start.line,
                              var.loc.start.column,
                              sp),
        ctx.Builder->GetInsertBlock());
    ctx.Builder->CreateStore(
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(*ctx.TheContext), this->init.getValue()),
        loopVar);
    ctx.Builder->CreateBr(loopBB);

    // enter loop
    fun->insert(fun->end(), loopBB);
    ctx.Builder->SetInsertPoint(loopBB);
    // 执行表达式
    auto loopE = this->body->gen_code(ctx);

    // 更新迭代变量，这里重新加载，确保在body中对变量的修改有效。
    const auto curValue = ctx.Builder->CreateLoad(llvm::Type::getDoubleTy(*ctx.TheContext), loopVar);
    if (this->step.has_value()) {
        const auto nextValue = ctx.Builder->CreateFAdd(
            curValue, llvm::ConstantFP::get(*ctx.TheContext, llvm::APFloat(this->step->getValue())),
            "next");
        ctx.Builder->CreateStore(nextValue, loopVar);
    } else {
        const auto nextValue = ctx.Builder->CreateFAdd(
            curValue, llvm::ConstantFP::get(*ctx.TheContext, llvm::APFloat(1.0)),
            "next");
        ctx.Builder->CreateStore(nextValue, loopVar);
    }

    // 判断是否结束
    auto loopEndCond = this->endCondition->gen_code(ctx);
    loopEndCond = ctx.Builder->CreateFCmpONE(loopEndCond, llvm::ConstantFP::get(*ctx.TheContext, llvm::APFloat(0.0)));
    // 更新loopB的实际出口block
    auto loopBBOut = ctx.Builder->GetInsertBlock();
    // jump
    ctx.Builder->CreateCondBr(loopEndCond, loopBB, loopEndBB);
    // loop结束了
    fun->insert(fun->end(), loopEndBB);
    ctx.Builder->SetInsertPoint(loopEndBB);
    auto loopRet = ctx.Builder->CreatePHI(llvm::Type::getDoubleTy(*ctx.TheContext),
                                          1, "loopret");
    loopRet->addIncoming(loopE, loopBBOut);
    ctx.ExitScope();
    // 结束ir生成

    return loopRet;
}

llvm::Value *VarExprAst::gen_code(CodeGenContext &ctx) {
    const auto fun = ctx.Builder->GetInsertBlock()->getParent();
    // debug信息
    llvm::DIScope *scope = ctx.TheCU;
    if (!ctx.lexicalBlocks.empty()) {
        scope = ctx.lexicalBlocks.back();
    }
    const auto sp = ctx.TheDIBuilder->createLexicalBlock(
        scope,
        ctx.TheCU->getFile(),
        this->getLocation().line,
        this->getLocation().column
    );

    // 生成ir
    ctx.EnterScope(sp);
    ctx.emitLocation(this);
    for (const auto &[id,expr]: this->variables) {
        const auto alloc = ctx.allocaVar(*fun, id.name);
        if (!alloc) {
            throw std::runtime_error{"try declare repeat name"};
        }
        const auto D = ctx.TheDIBuilder->createAutoVariable(
            sp,
            id.name,
            ctx.TheCU->getFile(),
            id.loc.start.line,
            ctx.doubleType,
            true
        );
        ctx.TheDIBuilder->insertDeclare(
            alloc,
            D,
            ctx.TheDIBuilder->createExpression(),
            llvm::DILocation::get(sp->getContext(), id.loc.start.line, id.loc.start.column, sp),
            ctx.Builder->GetInsertBlock());
        ctx.Builder->CreateStore(expr->gen_code(ctx), alloc);
    }
    const auto ret = body->gen_code(ctx);
    ctx.ExitScope();
    // 结束ir生成
    return ret;
}

llvm::Value *CallExprAst::gen_code(CodeGenContext &ctx) {
    llvm::Function *CalleeF = ctx.TheModule->getFunction(this->identifier.name);
    if (!CalleeF)
        throw std::runtime_error{"Unknown function referenced"};

    // If argument mismatch error.
    if (CalleeF->arg_size() != this->args.size())
        throw std::runtime_error("Incorrect # arguments passed");

    std::vector<llvm::Value *> ArgsV;
    for (unsigned i = 0, e = this->args.size(); i != e; ++i) {
        ArgsV.push_back(this->args[i]->gen_code(ctx));
        if (!ArgsV.back())
            return nullptr;
    }
    ctx.emitLocation(this);
    return ctx.Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}
