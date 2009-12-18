#include "Environment.h"

#include "Evaluator.h"
#include "Expr.h"
#include "DynamicObject.h"
#include "BlockPrimitives.h"
#include "EnvironmentPrimitives.h"
#include "NumberPrimitives.h"
#include "ObjectPrimitives.h"

namespace Finch
{
    Environment::Environment()
    {
        // build the global scope
        mGlobals = Ref<Scope>(new Scope()); 
        mCurrentScope = mGlobals;
        
        Ref<Object> rootObject = Object::NewObject(Ref<Object>(), "Object");
        mGlobals->Define("Object", rootObject);
        DynamicObject* objectObj = &static_cast<DynamicObject&>(*rootObject);
        objectObj->RegisterPrimitive("copy", ObjectCopy);
        objectObj->RegisterPrimitive("add-field:value:", ObjectAddFieldValue);
        objectObj->RegisterPrimitive("add-method:body:", ObjectAddMethodValue);

        // define Block prototype
        mBlock = Object::NewObject(rootObject, "Block");
        mGlobals->Define("Block", mBlock);
        
        DynamicObject* blockObj = &static_cast<DynamicObject&>(*mBlock);
        blockObj->RegisterPrimitive("value", BlockValue);
        
        // define Number prototype
        mNumber = Object::NewObject(rootObject, "Number");
        mGlobals->Define("Number", mNumber);
        
        DynamicObject* numberObj = &static_cast<DynamicObject&>(*mNumber);
        numberObj->RegisterPrimitive("abs", NumberAbs);
        numberObj->RegisterPrimitive("neg", NumberNeg);
        numberObj->RegisterPrimitive("+",   NumberPlus);
        numberObj->RegisterPrimitive("-",   NumberMinus);
        numberObj->RegisterPrimitive("*",   NumberMultiply);
        numberObj->RegisterPrimitive("/",   NumberDivide);
        numberObj->RegisterPrimitive("=",   NumberEquals);
        numberObj->RegisterPrimitive("!=",  NumberNotEquals);
        numberObj->RegisterPrimitive("<",   NumberLessThan);
        numberObj->RegisterPrimitive(">",   NumberGreaterThan);
        numberObj->RegisterPrimitive("<=",  NumberLessThanOrEqual);
        numberObj->RegisterPrimitive(">=",  NumberGreaterThanOrEqual);
        
        // define String prototype
        mString = Object::NewObject(rootObject, "String");
        mGlobals->Define("String", mString);
        
        // define nil
        mNil = Object::NewObject(rootObject, "Nil");
        mGlobals->Define("Nil", mNil);
        
        // define true and false
        mTrue = Object::NewObject(rootObject, "True");
        mGlobals->Define("True", mTrue);
        
        mFalse = Object::NewObject(rootObject, "False");
        mGlobals->Define("False", mFalse);
        
        // define Environment
        Ref<Object> environment = Object::NewObject(rootObject, "Environment");
        mGlobals->Define("Environment", environment);
        
        DynamicObject* environmentObj = &static_cast<DynamicObject&>(*environment);
        environmentObj->RegisterPrimitive("if:then:", EnvironmentIfThen);
        environmentObj->RegisterPrimitive("if:then:else:", EnvironmentIfThenElse);
        environmentObj->RegisterPrimitive("while:do:", EnvironmentWhileDo);
        environmentObj->RegisterPrimitive("write:", EnvironmentWrite);
        environmentObj->RegisterPrimitive("write-line:", EnvironmentWriteLine);
        environmentObj->RegisterPrimitive("load:", EnvironmentLoad);
    }
    
    Ref<Object> Environment::EvaluateBlock(Ref<Expr> expr)
    {
        mCurrentScope = Ref<Scope>(new Scope(mCurrentScope));
        
        Evaluator evaluator(*this);
        Ref<Object> result = evaluator.Evaluate(expr);

        mCurrentScope = mCurrentScope->Parent();
        
        return result;
    }
    
    Ref<Object> Environment::EvaluateMethod(Ref<Object> self, Ref<Expr> expr)
    {
        Ref<Object> previousSelf = mSelf;
        mSelf = self;
        
        Evaluator evaluator(*this);
        Ref<Object> result = evaluator.Evaluate(expr);
        
        mSelf = previousSelf;
        
        return result;
    }
}