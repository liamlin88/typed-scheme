//
// Created by Zhitao Lin on 2020/2/9.
//

#ifndef TYPED_SCHEME_SCHEMEOBJECT_HPP
#define TYPED_SCHEME_SCHEMEOBJECT_HPP

#include <memory>

enum class SchemeObjectType {
    CLOSURE
};

class SchemeObject{
public:
    SchemeObjectType schemeObjectType;
};

class Closure : public SchemeObject {
public:

    int instructionAddress{};
    map<string, string> boundVariables;
    map<string, string> freeVariables;
    map<string, bool> dirtyFlags;
    std::shared_ptr<Closure> parentClosurePtr;
    SchemeObjectType schemeObjectType = SchemeObjectType::CLOSURE;

    Closure() {};

    Closure(int instructionAddress, const shared_ptr<Closure> &parentClosurePtr) : instructionAddress(
            instructionAddress), parentClosurePtr(parentClosurePtr) {};

    void setBoundVariable(const string &variableName, const string &variableValue, bool dirtyFlag);

    string getBoundVariable(const string &variableName);

    bool hasBoundVariable(const string &variableName);

    void setFreeVariable(const string &variableName, const string &variableValue, bool dirtyFlag);

    string getFreeVariable(const string &variableName);

    bool hasFreeVariable(const string &variableName);
};

//=================================================================
//                    Closure's Closure
//=================================================================

void Closure::setBoundVariable(const string &variableName, const string &variableValue, bool dirtyFlag) {
    this->boundVariables[variableName] = variableValue;
    this->dirtyFlags[variableName] = dirtyFlag;
}

string Closure::getBoundVariable(const string &variableName) {
    return this->boundVariables[variableName];
}

void Closure::setFreeVariable(const string &variableName, const string &variableValue, bool dirtyFlag) {
    this->freeVariables[variableName] = variableValue;
    this->dirtyFlags[variableName] = dirtyFlag;
}

string Closure::getFreeVariable(const string &variableName) {
    return this->freeVariables[variableName];
}

bool Closure::hasBoundVariable(const string &variableName) {
    return this->boundVariables.count(variableName);
}

bool Closure::hasFreeVariable(const string &variableName) {
    return this->freeVariables.count(variableName);
}

#endif //TYPED_SCHEME_SCHEMEOBJECT_HPP