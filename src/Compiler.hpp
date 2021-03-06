//
// Created by zlin on 2020/3/8.
//

#ifndef TYPED_SCHEME_COMPILER_HPP
#define TYPED_SCHEME_COMPILER_HPP

#include <boost/algorithm/string.hpp>
#include <utility>
#include <vector>
#include <string>
#include "Parser.hpp"
#include "IrisObject.hpp"
#include "Instruction.hpp"

using namespace std;

string COMPILER_PREFIX = "_!!!compiler_prefix!!!_";
string COMPILER_PREFIX_TITLE = "Compile Error";

class Compiler {
public:
    AST ast;
    vector<Instruction> ILCode;
    string ERROR_PREFIX = "------------ Compile Error ------------";
    string ERROR_POSTFIX = "---------------------------------------";
    int ERROR_PREFIX_LEN = ERROR_PREFIX.size() - 1;

    int uniqueStrCounter = 0;

    explicit Compiler(AST ast) : ast(std::move(ast)) {};

    static vector<Instruction> compile(AST ast);

    void addInstruction(string inst);

    void beginCompile();

    void compileLambda(Handle lambdaHandle);

    void compileApplication(Handle handle);

    void compileQuasiquote(Handle handle);

    void addComment(string inst);

    void compileDefine(Handle handle);

    void compileComplexApplication(Handle handle);

    string makeUniqueString();

    void compileHos(HandleOrStr hos);

    void compileSet(Handle handle);

    void compileCond(Handle handle);

    void compileIf(Handle handle);

    void compileAnd(Handle handle);

    void compileOr(Handle handle);

    void compileFork(Handle handle);

    void compileCallCC(Handle handle);

    string createErrorMessage(string message, Handle handle);

    void handleArbitraryFunction(int j, const Handle &lambdaHandle);

    void compileApply(Handle handle);


    void checkWrongArgumentsNumberError(string functionName, int expectedNum, int actualNum, Handle handle);
};

vector<Instruction> Compiler::compile(AST ast) {
    Compiler compiler(std::move(ast));
    compiler.beginCompile();

    return compiler.ILCode;
}

void Compiler::addInstruction(string inst) {
    boost::trim(inst);
    Instruction instruction(inst);
    if (inst.empty()) {
        return;
    } else if (inst.length() >= 2 && inst[0] == ';' && inst[1] == ';') {
        // handle IL comments
        this->ILCode.push_back(instruction);
    } else {
        this->ILCode.push_back(instruction);
    }
}

void Compiler::addComment(string inst) {
    boost::trim(inst);
    Instruction instruction(inst);
    if (inst.empty()) {
        return;
    } else {
        this->addInstruction(";; " + inst);
    }
}

void Compiler::compileLambda(Handle lambdaHandle) {
    auto lambdaObjPtr = static_pointer_cast<LambdaObject>(this->ast.get(lambdaHandle));
    // label, for jumping
    this->addInstruction("@" + lambdaHandle);

    // in-order, fill the arguments
    for (int j = 0; j < lambdaObjPtr->parameters.size(); ++j) {
        this->addInstruction("store " + lambdaObjPtr->parameters[j]);

        // handle the '.' parameter, arbitrary arguments function
        // . should be follow by only one parameters
        // looks like: (lambda (arg0 arg1 . args) ())
        if (lambdaObjPtr->parameters[j].ends_with('.')) {
            // error will be raise inside, if something goes wrong
            this->handleArbitraryFunction(j, lambdaHandle);
        }
    }

    // execute and return the result(push the result to stack)
    for (int i = 0; i < lambdaObjPtr->bodies.size(); i++) {
        this->compileHos(lambdaObjPtr->bodies[i]);
    }

    this->addInstruction("return");
}

void Compiler::handleArbitraryFunction(int j, const Handle &lambdaHandle) {
    auto lambdaObjPtr = static_pointer_cast<LambdaObject>(this->ast.get(lambdaHandle));
    if (j + 1 != lambdaObjPtr->parameters.size() - 1) {
        if (j + 1 < lambdaObjPtr->parameters.size() - 1) {
            this->createErrorMessage(
                    "When using arbitrary arguments function, only one argument can be put after '.'.",
                    lambdaHandle);
            throw std::runtime_error("");
        } else if (j + 1 > lambdaObjPtr->parameters.size() - 1) {
            this->createErrorMessage(
                    "When using arbitrary arguments function, one argument should be put after '.'.",
                    lambdaHandle);
            throw std::runtime_error("");
        }
    }
}



void Compiler::compileHos(HandleOrStr hos) {
    Type hosType = typeOfStr(hos);

    if (hosType == Type::HANDLE) {
        auto schemeObjPtr = this->ast.get(hos);
        IrisObjectType schemeObjectType = schemeObjPtr->irisObjectType;

        if (schemeObjectType == IrisObjectType::LAMBDA) {
            this->addInstruction("loadclosure @" + hos);
        } else if (schemeObjectType == IrisObjectType::QUOTE || schemeObjectType == IrisObjectType::STRING) {
            this->addInstruction("push " + hos);
        } else if (schemeObjectType == IrisObjectType::QUASIQUOTE) {
            this->compileQuasiquote(hos);
        } else if (schemeObjectType == IrisObjectType::APPLICATION ||
                   schemeObjectType == IrisObjectType::UNQUOTE) {
            this->compileApplication(hos);
        }
    } else if (this->ast.isNativeCall(hos)) {
        this->addInstruction("push " + hos);
    } else if (hosType == Type::VARIABLE) {
        this->addInstruction("load " + hos);
    } else if (hosType == Type::UNDEFINED) {
        throw std::runtime_error("[compileHos] hos '" + hos + "'type is undefined");
    } else {
        // TYPE is number || boolean || symbol ||keyword || port || quote
        this->addInstruction("push " + hos);
    }
}

void Compiler::compileApplication(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));

    auto childrenHoses = applicationPtr->childrenHoses;

    if (childrenHoses.empty()) {
        return;
    }

    string first = childrenHoses[0];
    Type firstType = typeOfStr(first);
    if (first == "import") { return; }
    else if (first == "native") { return; }
    else if (first == "call/cc") { return this->compileCallCC(handle); }
    else if (first == "define") { return this->compileDefine(handle); }
    else if (first == "set!") { return this->compileSet(handle); }
    else if (first == "cond") { return this->compileCond(handle); }
    else if (first == "if") { return this->compileIf(handle); }
    else if (first == "and") { return this->compileAnd(handle); }
    else if (first == "or") { return this->compileOr(handle); }
    else if (first == "fork") { return this->compileFork(handle); }
    else if (first == "apply") {return this->compileApply(handle); }

    if (firstType == Type::HANDLE && this->ast.get(first)->irisObjectType == IrisObjectType::APPLICATION) {
        this->compileComplexApplication(handle);
        return;
    } else if (utils::makeSet<Type>(3, Type::HANDLE, Type::VARIABLE, Type::KEYWORD).count(firstType)) {

        string uniqueStr = this->makeUniqueString();
        this->addInstruction("pushend " + uniqueStr);
        // handle parameters
        for (int i = childrenHoses.size() - 1; i >= 1; i--) {
            this->compileHos(childrenHoses[i]);
            // handle parameters
        }
        this->addInstruction("pushend " + uniqueStr);


        // 1. Make sure the first child is valid: native, variable, primitive, lambda
        // 2. handle tailcall

        // Primitive
        // handle the expected parameters better
        if (firstType == Type::KEYWORD) {
            if (primitiveInstructionMap.count(first)) {
                this->addInstruction(primitiveInstructionMap[first]);
            } else {
                if (first == "list") {
                    if (childrenHoses.size() == 1) {
                        throw std::runtime_error(
                                "[compileApplication] list' arguments should more than 0.");
                    }
//                    this->addInstruction(
//                            "push " + to_string(childrenHoses.size() - 1)); // the number of list application arguments
                }
                this->addInstruction(first);
            }
        } else if (std::find(this->ast.tailcalls.begin(), this->ast.tailcalls.end(), handle) !=
                   this->ast.tailcalls.end()) {
            // we don't has tailcalls right now
            if (firstType == Type::HANDLE && this->ast.get(first)->irisObjectType == IrisObjectType::LAMBDA) {
                this->addInstruction("tailcall " + first);
            } else if (firstType == Type::VARIABLE) {
                // include native
                this->addInstruction("tailcall " + first);
            } else {
                throw std::runtime_error("[compileApplication] the first argument is not callable.");
            }
        } else {
            if (firstType == Type::HANDLE && this->ast.get(first)->irisObjectType == IrisObjectType::LAMBDA) {
                auto lambdaObjPtr = static_pointer_cast<LambdaObject>(this->ast.get(first));
                this->addInstruction("call @" + first);
            } else if (firstType == Type::VARIABLE) {
                // include native
                this->addInstruction("call " + first);
            } else {
                throw std::runtime_error("[compileApplication] the first argument is not callable.");
            }
        }
    }
}

// eta-conversion
// (A 1 2 ..) → ((lambda (F x y ..) (F x y ..)) A 1 2 ..)
void Compiler::compileComplexApplication(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));

    vector<HandleOrStr> childrenHoses = applicationPtr->childrenHoses;

    string uniqueStr = this->makeUniqueString();

    string entryLabel = "@COMPLEX_APP_" + uniqueStr;
    this->addInstruction("goto " + entryLabel);

    // ------------------------------------------------------- TMP LAMBDA ----------------------------
    // a temporary lambda function (lambda (F x y ..) (F x y ..))
    string tmpLambdaLabel = "@TMP_LAMBDA_" + uniqueStr;
    this->addInstruction(tmpLambdaLabel);

    vector<string> tmpLambdaParams;
    for (int i = 0; i < childrenHoses.size(); ++i) {
        tmpLambdaParams.push_back("TEMP_LAMBDA_PARAM" + to_string(i) + "_" + uniqueStr);
    }

    for (int i = 0; i < childrenHoses.size(); ++i) {
        this->addInstruction("store " + tmpLambdaParams[i]);
    }

    for (int i = childrenHoses.size() - 1; i >= 1; --i) {
        this->addInstruction("load " + tmpLambdaParams[i]);
    }

    // tmpLambdaParams[0] is always a Handle(Application)!!
    // call it before further execution
    this->addInstruction("tailcall " + tmpLambdaParams[0]);
    this->addInstruction("return");
    // ------------------------------------------------------- TMP LAMBDA ----------------------------

    this->addInstruction(entryLabel);
    // Compile : (tmp_lambda A 1 2 ..)
    for (int j = childrenHoses.size() - 1; j >= 0; --j) {
        this->compileHos(childrenHoses[j]);
    }

    // call the tmp lambda
    this->addInstruction("call " + tmpLambdaLabel);

}

void Compiler::compileDefine(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));

    auto childrenHoses = applicationPtr->childrenHoses;

    if (childrenHoses.size() != 3) {
        string errorMessage = utils::createArgumentsNumberErrorMessage("define", 3, childrenHoses.size());
        utils::raiseError(ast, handle, errorMessage, COMPILER_PREFIX_TITLE);
    }

    if (typeOfStr(childrenHoses[1]) != Type::VARIABLE) {
        throw std::runtime_error(
                "[compileDefine] define's first argument " + childrenHoses[1] + " should be a variable but not a " +
                TypeStrMap[typeOfStr(childrenHoses[1])]);
    }

    if (typeOfStr(childrenHoses[2]) == Type::HANDLE) {
        auto schemeObjPtr = this->ast.get(childrenHoses[2]);

        if (schemeObjPtr->irisObjectType == IrisObjectType::LAMBDA) {
            this->addInstruction("push @" + childrenHoses[2]); // go to the label
        } else if (schemeObjPtr->irisObjectType == IrisObjectType::QUOTE) {
            this->addInstruction("push " + childrenHoses[2]);
        } else if (schemeObjPtr->irisObjectType == IrisObjectType::QUASIQUOTE) {
            this->compileQuasiquote(childrenHoses[2]);
        } else if (schemeObjPtr->irisObjectType == IrisObjectType::STRING) {
            this->addInstruction("push " + childrenHoses[2]);
        } else if (schemeObjPtr->irisObjectType == IrisObjectType::APPLICATION ||
                   schemeObjPtr->irisObjectType == IrisObjectType::UNQUOTE) {
            this->compileApplication(childrenHoses[2]);
        } else {
            throw std::runtime_error("[compileDefine] define's second argument " + childrenHoses[2] + " is invalid");
        }
    } else if (utils::makeSet<Type>(4, Type::NUMBER, Type::BOOLEAN, Type::KEYWORD, Type::PORT).count(
            typeOfStr(childrenHoses[2])) || this->ast.isNativeCall(childrenHoses[2])) {
        this->addInstruction("push " + childrenHoses[2]);
    } else if (typeOfStr(childrenHoses[2]) == Type::VARIABLE) {
        this->addInstruction("load " + childrenHoses[2]);
    } else {
        throw std::runtime_error("[compileDefine] define's second argument " + childrenHoses[2] + " is invalid");
    }

    // store
    this->addInstruction("store " + childrenHoses[1]);


}

void Compiler::compileQuasiquote(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));
    auto childrenHoses = applicationPtr->childrenHoses;

    for (int i = 0; i < childrenHoses.size(); ++i) {
        HandleOrStr child = childrenHoses[i];
        this->compileHos(child);
    }

    this->addInstruction("push " + to_string(childrenHoses.size()));
    this->addInstruction("concat");

}

void Compiler::compileSet(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));

    auto childrenHoses = applicationPtr->childrenHoses;

    if (childrenHoses.size() != 3) {
        throw std::runtime_error("[compileSet] set " + handle + " should have only two children");
    }

    // push or load
    string rightHos = childrenHoses[2];
    this->compileHos(rightHos);

    //set
    string leftHos = childrenHoses[1];
    Type leftHosType = typeOfStr(leftHos);

    if (leftHosType == Type::VARIABLE) {
        this->addInstruction("set " + leftHos);
    } else {
        throw std::runtime_error("[compileSet] set's first argument " + leftHos + " should be a variable");
    }
}

void Compiler::compileCond(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));

    auto childrenHoses = applicationPtr->childrenHoses;

    string uniqueStr = this->makeUniqueString();

    for (int i = 1; i < childrenHoses.size(); ++i) {
        shared_ptr<ApplicationObject> clausePtr = static_pointer_cast<ApplicationObject>(
                this->ast.get(childrenHoses[i]));

        this->addInstruction("@COND_BRANCH_" + uniqueStr + "_" + to_string(i));

        HandleOrStr predicate = clausePtr->childrenHoses[0];

        if (predicate != "else") {
            Type predicateType = typeOfStr(predicate);
            if (predicateType == Type::HANDLE) {
                auto predicateObjPtr = this->ast.get(predicate);
                if (predicateObjPtr->irisObjectType == IrisObjectType::APPLICATION) {
                    this->compileApplication(predicate);
                } else {
                    // push all, for other situation
                    this->addInstruction("push " + predicate);
                }
            } else {
                // for other situation where predicate is not HANDLE!!
                this->compileHos(predicate);
            }
            // Handle if predicate is false
            if (i == childrenHoses.size() - 1) {
                // go to the end, if i is the last branch
                this->addInstruction("iffalse @COND_END_" + uniqueStr);
            } else {
                // go to the next branch
                this->addInstruction("iffalse @COND_BRANCH_" + uniqueStr + "_" + to_string(i + 1));
            }
        }

        HandleOrStr branchBody = clausePtr->childrenHoses[1];
        this->compileHos(branchBody);

        if (predicate == "else" || i == childrenHoses.size() - 1) {
            this->addInstruction("@COND_END_" + uniqueStr);
            break; // ignore every branch behind else branch
        } else {
            this->addInstruction("goto @COND_END_" + uniqueStr);
        }

    }

}

void Compiler::compileIf(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));
    auto childrenHoses = applicationPtr->childrenHoses;

    if (childrenHoses.size() != 4) {
        throw std::runtime_error("[compileIf] if " + handle + " should have four children");
    }

    HandleOrStr predicate = childrenHoses[1];
    Type predicateType = typeOfStr(predicate);
    if (predicateType == Type::HANDLE) {
        auto predicateObjPtr = this->ast.get(predicate);
        if (predicateObjPtr->irisObjectType == IrisObjectType::APPLICATION) {
            this->compileApplication(predicate);
        } else {
            // push all, for other situation
            this->addInstruction("push " + predicate);
        }
    } else {
        // for other situation where predicate is not HANDLE!!
        this->compileHos(predicate);
    }

    string uniqueStr = this->makeUniqueString();
    string trueLabel = "@IF_TRUE_" + uniqueStr;
    string endLabel = "@IF_END_" + uniqueStr;

    this->addInstruction("iftrue " + trueLabel); // if true go to trueLabel

    // ----- False Branch ------
    HandleOrStr falseBranch = childrenHoses[3];
    this->compileHos(falseBranch);

    this->addInstruction("goto " + endLabel); // false branch done here
    // ----- False Branch ------

    // ----- True Branch -------
    this->addInstruction(trueLabel);

    HandleOrStr trueBranch = childrenHoses[2];
    this->compileHos(trueBranch);
    // ----- True Branch -------

    this->addInstruction(endLabel);

}

void Compiler::compileAnd(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));
    auto childrenHoses = applicationPtr->childrenHoses;

    if (childrenHoses.size() != 3) {
        throw std::runtime_error("[compileAnd] " + handle + " should have three children");
    }

    string uniqueStr = this->makeUniqueString();
    string endLabel = "@AND_END_" + uniqueStr;
    string falseLabel = "@AND_FALSE_" + uniqueStr;

    for (int i = 1; i < childrenHoses.size(); ++i) {
        HandleOrStr child = childrenHoses[i];
        this->compileHos(child);

        this->addInstruction("iffalse " + falseLabel);
    }

    //True
    this->addInstruction("push #t");
    this->addInstruction("goto " + endLabel);

    //False
    this->addInstruction(falseLabel);
    this->addInstruction("push #f");

    this->addInstruction(endLabel);
}

void Compiler::compileOr(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));
    auto childrenHoses = applicationPtr->childrenHoses;

    if (childrenHoses.size() != 3) {
        throw std::runtime_error("[compileOr] " + handle + " should have three children");
    }

    string uniqueStr = this->makeUniqueString();
    string endLabel = "@OR_END_" + uniqueStr;
    string trueLabel = "@OR_TRUE_" + uniqueStr;

    for (int i = 1; i < childrenHoses.size(); ++i) {
        HandleOrStr child = childrenHoses[i];
        this->compileHos(child);

        this->addInstruction("iftrue " + trueLabel);
    }

    // False
    this->addInstruction("push #f");
    this->addInstruction("goto " + endLabel);

    // True
    this->addInstruction(trueLabel);
    this->addInstruction("push #t");

    this->addInstruction(endLabel);
}

void Compiler::compileFork(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));
    auto childrenHoses = applicationPtr->childrenHoses;

    this->checkWrongArgumentsNumberError("Fork", 2, childrenHoses.size(), handle);

    this->addInstruction("fork " + childrenHoses[1]);
}

void Compiler::compileApply(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));
    auto childrenHoses = applicationPtr->childrenHoses;

    this->checkWrongArgumentsNumberError("Apply", 3, childrenHoses.size(), handle);

    string uniqueStr = this->makeUniqueString();
    this->addInstruction("pushend " + uniqueStr);
    this->compileHos(childrenHoses[2]);
    this->addInstruction("pushend " + uniqueStr);
    this->addInstruction("pushlist");
    // TODO: apply a complex application
//    this->addInstruction("call " + handle);

    HandleOrStr first = childrenHoses[1];
    Type firstType = typeOfStr(first);
    if (first == "import") { return; }
    else if (first == "native") { return; }
    else if (first == "call/cc") { return this->compileCallCC(handle); }
    else if (first == "define") { return this->compileDefine(handle); }
    else if (first == "set!") { return this->compileSet(handle); }
    else if (first == "cond") { return this->compileCond(handle); }
    else if (first == "if") { return this->compileIf(handle); }
    else if (first == "and") { return this->compileAnd(handle); }
    else if (first == "or") { return this->compileOr(handle); }
    else if (first == "fork") { return this->compileFork(handle); }
    else if (first == "apply") {return this->compileApply(handle); }

    if (firstType == Type::HANDLE && this->ast.get(first)->irisObjectType == IrisObjectType::APPLICATION) {
        this->compileComplexApplication(handle);
        return;
    } else if (utils::makeSet<Type>(3, Type::HANDLE, Type::VARIABLE, Type::KEYWORD).count(firstType)) {
        // 1. Make sure the first child is valid: native, variable, primitive, lambda
        // 2. handle tailcall

        // Primitive
        // handle the expected parameters better
        if (firstType == Type::KEYWORD) {
            if (primitiveInstructionMap.count(first)) {
                this->addInstruction(primitiveInstructionMap[first]);
            } else {
                if (first == "list") {
                    if (childrenHoses.size() == 1) {
                        throw std::runtime_error(
                                "[compileApplication] list' arguments should more than 0.");
                    }
//                    this->addInstruction(
//                            "push " + to_string(childrenHoses.size() - 1)); // the number of list application arguments
                }
                this->addInstruction(first);
            }
        } else if (std::find(this->ast.tailcalls.begin(), this->ast.tailcalls.end(), handle) !=
                   this->ast.tailcalls.end()) {
            // we don't has tailcalls right now
            if (firstType == Type::HANDLE && this->ast.get(first)->irisObjectType == IrisObjectType::LAMBDA) {
                this->addInstruction("tailcall " + first);
            } else if (firstType == Type::VARIABLE) {
                // include native
                this->addInstruction("tailcall " + first);
            } else {
                throw std::runtime_error("[compileApplication] the first argument is not callable.");
            }
        } else {
            if (firstType == Type::HANDLE && this->ast.get(first)->irisObjectType == IrisObjectType::LAMBDA) {
                auto lambdaObjPtr = static_pointer_cast<LambdaObject>(this->ast.get(first));
                this->addInstruction("call @" + first);
            } else if (firstType == Type::VARIABLE) {
                // include native
                this->addInstruction("call " + first);
            } else {
                throw std::runtime_error("[compileApplication] the first argument is not callable.");
            }
        }
    }
}


void Compiler::compileCallCC(Handle handle) {
    shared_ptr<ApplicationObject> applicationPtr = static_pointer_cast<ApplicationObject>(this->ast.get(handle));
    auto childrenHoses = applicationPtr->childrenHoses;

    if (childrenHoses.size() != 2) {
        throw std::runtime_error("[compileFork] " + handle + " should have two children");
    }

    HandleOrStr thunk = childrenHoses[1];
    Type thunkType = typeOfStr(thunk);

    string contLabel = "CC_" + thunk + "_" + this->makeUniqueString();

    this->addInstruction("capturecc " + contLabel);
    this->addInstruction("load " + contLabel);

    if (thunkType == Type::HANDLE) {
        shared_ptr<IrisObject> schemeObjPtr = this->ast.get(thunk);
        if (schemeObjPtr->irisObjectType == IrisObjectType::LAMBDA) {
            this->addInstruction("call @" + thunk);
        } else {
            throw "[compileCallCC] call/cc's argument must be a thunk";
        }
    } else if (thunkType == Type::VARIABLE) {
        this->addInstruction("call " + thunk);
    } else {
        throw "[compileCallCC] call/cc's argument must be a thunk";
    }
}

string Compiler::makeUniqueString() {
    string uniqueStr = this->ast.moduleName + ".UniqueStrID" + to_string(this->uniqueStrCounter);
    this->uniqueStrCounter++;
    return uniqueStr;
}


void Compiler::beginCompile() {
    this->addInstruction(";; IrisCompiler GOGOGO");
    this->addInstruction("call @" + this->ast.getTopLambdaHandle());
    this->addInstruction("halt");

    // ( (lambda () ( bodies ) )
    for (auto lambdaHandle : this->ast.getLambdaHandles()) {
        this->compileLambda(lambdaHandle);
    }

    for (auto &inst : this->ILCode) {
        cout << inst.instructionStr << endl;
    }
}

string Compiler::createErrorMessage(string message, Handle handle) {
    string prefix = this->ERROR_PREFIX;
    cout << this->ERROR_PREFIX << endl;
    utils::coutContext(this->ast, handle, message);
    cout << this->ERROR_POSTFIX<< endl;
}

void Compiler::checkWrongArgumentsNumberError(string functionName, int expectedNum, int actualNum, Handle handle) {
    if (expectedNum != actualNum) {
        string be = actualNum > 1 ? " are " : " is ";
        string add_s = expectedNum > 1 ? "s" : "";
        string message = "[" + functionName + "] expects " + to_string(expectedNum) + " argument" + add_s +
                ", " +
                to_string(actualNum) + be + "given";
        this->createErrorMessage(message, handle);
        throw std::runtime_error("");
    }
}


#endif //TYPED_SCHEME_COMPILER_HPP
